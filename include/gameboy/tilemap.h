/*
	@author Natesh Narain <nnaraindev@gmail.com>
	@date Spet 15, 2016
*/

#ifndef GAMEBOY_TILEMAP_H
#define GAMEBOY_TILEMAP_H

#include "gameboy/tileram.h"
#include "gameboy/lcd_controller.h"
#include "gameboy/memorymap.h"

#include <cstdint>

namespace gb
{
	/**
		\brief Class that knows how to render background map data
	*/
	class TileMap
	{
	public:
		/* Map types */
		enum class Map
		{
			BACKGROUND     = (1<<3),
			WINDOW_OVERLAY = (1<<6)
		};

	public:

		// TODO: remove this constructor
		TileMap(MMU& mmu);

		~TileMap();

		std::vector<Tile> getMapData(TileRAM tileram, Map map) const;
		std::vector<Tile> getMapData(TileRAM tileram, uint16_t start, uint16_t end) const;

		TileRAM::TileLine getTileLine(int line, Map map);

	private:
		uint16_t getAddress(Map map);

	private:
		TileRAM tileram_;
		const MMU& mmu_;

		uint8_t& scx_;
		uint8_t& scy_;
	};
}

#endif // GAMEBOY_TILEMAP_H
