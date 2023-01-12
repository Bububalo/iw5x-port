﻿#include <std_include.hpp>

#include "igfximage.hpp"
#include "../asset_dumper.hpp"

#include "utils/io.hpp"
#include "utils/stream.hpp"
#include "utils/string.hpp"
#include "utils/hook.hpp"

#include "module/console.hpp"
#include "module/command.hpp"

#define IW4X_IMG_VERSION "0"
namespace asset_dumpers
{

	void igfximage::convert(const game::native::XAssetHeader& header, iw4::native::XAssetHeader& out)
	{
		assert(header.image);
		out.image = reinterpret_cast<iw4::native::GfxImage*>(header.image);

		out.image->delayLoadPixels = false; // 🤔
	}

	void igfximage::write(const iw4::native::XAssetHeader& header)
	{
		auto image = header.image;
		std::string name = image->name;

		if (image->category != game::native::GfxImageCategory::IMG_CATEGORY_LOAD_FROM_FILE && image->texture.loadDef)
		{
			if (name[0] == '*') name.erase(name.begin());

			utils::stream buffer;
			buffer.saveArray("IW4xImg" IW4X_IMG_VERSION, 8); // just stick version in the magic since we have an extra char

			buffer.saveObject(static_cast<unsigned char>(image->mapType));
			buffer.saveObject(image->semantic);
			buffer.saveObject(image->category);

			buffer.saveObject(image->texture.loadDef->resourceSize);

			buffer.saveObject(image->texture.loadDef->levelCount);
			buffer.saveObject(static_cast<unsigned char>(image->texture.loadDef->flags));
			buffer.saveObject(image->width);
			buffer.saveObject(image->height);
			buffer.saveObject(image->depth);
			buffer.saveObject(image->texture.loadDef->format);
			buffer.saveObject(image->texture.loadDef->resourceSize); // Yes, again

			buffer.save(image->texture.loadDef->data, image->texture.loadDef->resourceSize);

			utils::io::write_file(std::format("{}/images/{}.iw4xImage", get_export_path(), name), buffer.toBuffer());
		}
		else
		{
			const auto iwi = std::format("images/{}.iwi", image->name);
			// > 10MB cannot use FS_FileRead (breaks hunk)
			auto contents = game::native::filesystem_read_big_file(iwi.data(), game::native::FS_THREAD_DATABASE);

			if (contents.empty())
			{
				// Ignore that
				if (std::string(image->name).starts_with("watersetup")) {
					return;
				}

				console::info("Image %s not found, mapping to normalmap!\n", name.data());

				contents = game::native::filesystem_read_big_file("images/$identitynormalmap.iwi", game::native::FS_THREAD_DATABASE);
			}

			if (contents.size() > 0)
			{
				utils::io::write_file(std::format("{}/images/{}.iwi", get_export_path(), image->name), contents);
			}
			else
			{
				console::info("Unable to map to normalmap, this should not happen!\n");
			}
		}
	}

	int igfximage::store_texture_hk()
	{
		game::native::GfxImageLoadDef** loadDef = *reinterpret_cast<game::native::GfxImageLoadDef***>(0x13E2940);
		game::native::GfxImage* image = *reinterpret_cast<game::native::GfxImage**>(0X13E2410);

		size_t size = 16 + (*loadDef)->resourceSize;
		void* data = utils::memory::allocate(size);
		std::memcpy(data, *loadDef, size);

		image->texture.loadDef = reinterpret_cast<game::native::GfxImageLoadDef*>(data);

		return 0;
	}

	void igfximage::release_texture_hk(game::native::XAssetHeader header)
	{
		if (header.image && header.image->texture.loadDef)
		{
			utils::memory::free(header.image->texture.loadDef);
		}
	}

	igfximage::igfximage()
	{
		command::add("dumpGfxImage", [&](const command::params& params)
			{
				if (params.size() < 2) return;

				auto name = params[1];

				if (name == "*"s)
				{
					std::vector<game::native::XAssetHeader> headers{};

					game::native::DB_EnumXAssets(game::native::XAssetType::ASSET_TYPE_IMAGE, [](game::native::XAssetHeader header, void* data) {
						auto headers = reinterpret_cast<std::vector<game::native::XAssetHeader>*>(data);
						
						headers->push_back(header);

					}, &headers, false);

					for (auto header : headers)
					{
						dump(header, true);
					}
				}
				else
				{
					auto header = game::native::DB_FindXAssetHeader(game::native::XAssetType::ASSET_TYPE_IMAGE, name, false);

					if (header.data)
					{
						dump(header, true);
						console::info("successfullly dumped image %s!\n", name);
					}
					else
					{
						console::info("could not dump image %s from the database (cannot find it)\n", name);
					}
				}
			});

		utils::hook(0x66A700, igfximage::store_texture_hk, HOOK_JUMP).install()->quick();
		utils::hook(0x66A1C0, igfximage::release_texture_hk, HOOK_JUMP).install()->quick();
	}

}