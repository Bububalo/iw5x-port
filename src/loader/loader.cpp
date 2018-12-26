#include <std_include.hpp>
#include "loader.hpp"
#include "binary_loader.hpp"
#include "utils/string.hpp"

loader::loader(const launcher::mode mode) : mode_(mode)
{
}

FARPROC loader::load(const utils::nt::module& module) const
{
	const auto buffer = binary_loader::load(this->mode_);
	if (buffer.empty()) return nullptr;

	utils::nt::module source(HMODULE(buffer.data()));
	if (!source) return nullptr;

	this->load_sections(module, source);
	this->load_imports(module, source);

	if (source.get_optional_header()->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size)
	{
		const IMAGE_TLS_DIRECTORY* target_tls = reinterpret_cast<PIMAGE_TLS_DIRECTORY>(module.get_ptr() + module
		                                                                                                  .get_optional_header()
		                                                                                                  ->
		                                                                                                  DataDirectory
			[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);
		const IMAGE_TLS_DIRECTORY* source_tls = reinterpret_cast<PIMAGE_TLS_DIRECTORY>(module.get_ptr() + source
		                                                                                                  .get_optional_header()
		                                                                                                  ->
		                                                                                                  DataDirectory
			[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);

		*reinterpret_cast<DWORD*>(source_tls->AddressOfIndex) = 0;

		DWORD old_protect;
		VirtualProtect(PVOID(target_tls->StartAddressOfRawData),
		               source_tls->EndAddressOfRawData - source_tls->StartAddressOfRawData, PAGE_READWRITE,
		               &old_protect);

		const auto tls_base = *reinterpret_cast<LPVOID*>(__readfsdword(0x2C));
		std::memmove(tls_base, PVOID(source_tls->StartAddressOfRawData),
		             source_tls->EndAddressOfRawData - source_tls->StartAddressOfRawData);
		std::memmove(PVOID(target_tls->StartAddressOfRawData), PVOID(source_tls->StartAddressOfRawData),
		             source_tls->EndAddressOfRawData - source_tls->StartAddressOfRawData);
	}

	DWORD oldProtect;
	VirtualProtect(module.get_nt_headers(), 0x1000, PAGE_EXECUTE_READWRITE, &oldProtect);

	module.get_optional_header()->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT] = source
	                                                                            .get_optional_header()->DataDirectory[
		IMAGE_DIRECTORY_ENTRY_IMPORT];
	std::memmove(module.get_nt_headers(), source.get_nt_headers(),
	             sizeof(IMAGE_NT_HEADERS) + (source.get_nt_headers()->FileHeader.NumberOfSections * (sizeof(
		             IMAGE_SECTION_HEADER))));

	return FARPROC(module.get_ptr() + source.get_relative_entry_point());
}

void loader::set_import_resolver(const std::function<FARPROC(const std::string&, const std::string&)>& resolver)
{
	this->import_resolver_ = resolver;
}

void loader::load_section(const utils::nt::module& target, const utils::nt::module& source,
                          IMAGE_SECTION_HEADER* section)
{
	void* target_ptr = target.get_ptr() + section->VirtualAddress;
	const void* source_ptr = source.get_ptr() + section->PointerToRawData;

	if (PBYTE(target_ptr) >= (target.get_ptr() + BINARY_PAYLOAD_SIZE))
	{
		throw std::runtime_error("Section exceeds the binary payload size, please increase it!");
	}

	if (section->SizeOfRawData > 0)
	{
		const auto size_of_data = std::min(section->SizeOfRawData, section->Misc.VirtualSize);
		std::memmove(target_ptr, source_ptr, size_of_data);

		DWORD old_protect;
		VirtualProtect(target_ptr, size_of_data, PAGE_EXECUTE_READWRITE, &old_protect);
	}
}

void loader::load_sections(const utils::nt::module& target, const utils::nt::module& source) const
{
	for (auto& section : source.get_section_headers())
	{
		this->load_section(target, source, section);
	}
}

void loader::load_imports(const utils::nt::module& target, const utils::nt::module& source) const
{
	IMAGE_DATA_DIRECTORY* import_directory = &source.get_optional_header()->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

	auto descriptor = PIMAGE_IMPORT_DESCRIPTOR(target.get_ptr() + import_directory->VirtualAddress);

	while (descriptor->Name)
	{
		std::string name = LPSTR(target.get_ptr() + descriptor->Name);

		auto name_table_entry = reinterpret_cast<uintptr_t*>(target.get_ptr() + descriptor->OriginalFirstThunk);
		auto address_table_entry = reinterpret_cast<uintptr_t*>(target.get_ptr() + descriptor->FirstThunk);

		if (!descriptor->OriginalFirstThunk)
		{
			name_table_entry = reinterpret_cast<uintptr_t*>(target.get_ptr() + descriptor->FirstThunk);
		}

		while (*name_table_entry)
		{
			FARPROC function = nullptr;
			std::string function_name;

			// is this an ordinal-only import?
			if (IMAGE_SNAP_BY_ORDINAL(*name_table_entry))
			{
				auto module = utils::nt::module::load(name);
				if (module)
				{
					function = GetProcAddress(module, MAKEINTRESOURCEA(IMAGE_ORDINAL(*name_table_entry)));
				}

				function_name = "#" + std::to_string(IMAGE_ORDINAL(*name_table_entry));
			}
			else
			{
				auto import = PIMAGE_IMPORT_BY_NAME(target.get_ptr() + *name_table_entry);
				function_name = import->Name;

				if (this->import_resolver_) function = this->import_resolver_(name, function_name);
				if (!function)
				{
					auto module = utils::nt::module::load(name);
					if (module)
					{
						function = GetProcAddress(module, function_name.data());
					}
				}
			}

			if (!function)
			{
				throw std::runtime_error(utils::string::va("Unable to load import '%s' from module '%s'",
				                                           function_name.data(), name.data()));
			}

			*address_table_entry = reinterpret_cast<uintptr_t>(function);

			name_table_entry++;
			address_table_entry++;
		}

		descriptor++;
	}
}
