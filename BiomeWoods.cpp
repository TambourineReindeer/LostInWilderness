
#include "BiomeWoods.h"
#include "Block.h"
#include "BlockDirt.h"
#include "BlockSaltWater.h"
#include "Chunk.h"
#include "Noise.h"

#include <cstdlib>
#include <cmath>

using namespace Noise;


void BiomeWoods::generate( Chunk *chunk, Perlin *noise )
{
	// TODO: finish
	for(int64_t x = 0; x < CHUNK_WIDTH; x++)
	{
		for(int64_t z = 0; z < CHUNK_LENGTH; z++)
		{
			// Land height
			int64_t rx = x + (chunk->ax * CHUNK_WIDTH);
			int64_t rz = z + (chunk->az * CHUNK_LENGTH);

			int64_t height = 64 + worldgen->getTerrainHeight( rx, rz, 0.0f );

			// Land blocks
			for (int64_t y = 0; y < CHUNK_HEIGHT; y++ )
			{
				int actual_y = y + chunk->ay * CHUNK_HEIGHT;

				// -- Sea level
				if ( actual_y >= height && actual_y < 64 )
				{
					BlockSaltWater b(ResourceManager::iResourceManager->getBlockInfo("liw:salt_water"));
					chunk->set( x, y, z, &b );
					continue;
				}

				// Are we above "ground" level?
				if ( y + chunk->ay * CHUNK_HEIGHT >= height )
				{
					Block *b = chunk->get(x, y-1, z);

					// Place a tree if the block below is a dirt block
					if( b != nullptr &&
						b->info == ResourceManager::iResourceManager->getBlockInfo( "liw:soil" ) )
					{
						int rv = (rand() % 1024);
						if ( rv == 8)
							this->placeBoulder( chunk, x, y, z );
						else if ( rv <= 5 )
							this->placePineTree( chunk, x, y, z );
					}
					continue;
				}

				if ( y < 4 )
				{
					Block b(ResourceManager::iResourceManager->getBlockInfo("liw:bedrock"));
					chunk->set(x,y,z, &b ); // bedrock
				}
				else if ( y < height - 4 )
				{
					Block b(ResourceManager::iResourceManager->getBlockInfo("liw:stone"));
					chunk->set(x,y,z, &b );
				}
				else
				{
					Block b(ResourceManager::iResourceManager->getBlockInfo("liw:soil"));
					chunk->set(x,y,z, &b );
				}
			}
		}
	}
}

void BiomeWoods::placePineTree( Chunk *chunk, int x, int y, int z )
{
	Block wood(ResourceManager::iResourceManager->getBlockInfo("liw:wood"));
	Block leaves(ResourceManager::iResourceManager->getBlockInfo("liw:leaves"));
	// -- Set the height of the trunk
	int h = 2 + (rand() % 2);
	int mh = 4 + (rand() % 6);

	for ( int iy = 0; iy <  mh; iy++ )
	{
		for ( int ix = -2; ix <= 2; ix++ )
		for ( int iz = -2; iz <= 2; iz++ )
		{
			float l = 6.0f * ( 1.0f - ( float(iy) / float(mh) ) );
			if ( float(ix * ix + iz * iz) < l )
			chunk->set( x + ix, y + h + iy, z + iz, &leaves );
		}
	}

	// -- Place the trunk
	for ( int iy = 0; iy <= h + (mh/2); iy++ )
	{
		chunk->set(x, y + iy, z, &wood);
	}
}

void BiomeWoods::placeBoulder( Chunk *chunk, int x, int y, int z )
{
	Block stone(ResourceManager::iResourceManager->getBlockInfo("liw:stone"));

	for ( int iy = -3; iy <= 3; iy++ )
	{
		for ( int ix = -3; ix <= 3; ix++ )
		for ( int iz = -3; iz <= 3; iz++ )
		{
			if ( ix * ix + iz * iz + iy * iy < 6 )
			chunk->set( x + ix, y + iy, z + iz, &stone );
		}
	}
}
