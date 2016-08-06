#include <stdlib.h>
#include <3ds.h>
#include <sf2d.h>
#include <math.h>
#include <string.h>

#define TWIDTH 400
#define THEIGHT 240

uint8_t readMap(uint8_t* map, uint8_t w, uint8_t h, uint8_t x, uint8_t y)
{
	return map[(y%h)*w+(x%w)]; // the % are useful in case we want to read at "x-1" and x may be an unsigned 0, well we can read at "x+w-1" and the modulo fixes stuff
}

uint16_t generateLevel(uint8_t* m, uint8_t w, uint8_t h, uint16_t length)
{
	uint16_t d = w*h;

	uint8_t ver[] = {1,1,0,2};
	uint8_t hor[] = {0,2,1,1};

	uint8_t tries = 0;
	uint8_t best[d];
	uint16_t l=0, maxl=0;
	do {
		for (uint16_t i=0; i<d; i++) m[i]=0;

		tries++;
		uint8_t x=0, y=0;
		l=0;

		while (l<length) {
			if (readMap(m,w,h,x+1,y)==3 && readMap(m,w,h,x+w-1,y)==3 && readMap(m,w,h,x,y+1)==3 && readMap(m,w,h,x,y+h-1)==3) break;
			l++;

			uint8_t newX=0, newY=0;
			do {
				uint8_t k = rand()%4;
				newX = (x+w+hor[k]-1)%w;
				newY = (y+h+ver[k]-1)%h;
			} while (readMap(m,w,h,newX,newY)==3);

			x = newX;
			y = newY;
			m[y*w+x]++;
		}

		if (l>maxl) {
			maxl = l;
			memcpy(best, m, d);
		}
	} while (tries<15);

	memcpy(best, m, d);
	return maxl;
}

u32 greyed(u32 c1)
{
	uint8_t r1 = c1;
	uint8_t g1 = c1>>8;
	uint8_t b1 = c1>>16;
	uint8_t a1 = c1>>24;
	uint8_t c = (r1+g1+b1)/3;
	return (a1<<24) | (c<<16) | (c<<8) | c;
}

u32 interpolate(u32 c1, u32 c2, float t)
{
	uint8_t r1 = c1;
	uint8_t g1 = c1>>8;
	uint8_t b1 = c1>>16;
	uint8_t a1 = c1>>24;
	uint8_t r2 = c2;
	uint8_t g2 = c2>>8;
	uint8_t b2 = c2>>16;
	uint8_t a2 = c2>>24;
	uint8_t r3 = t*(float)r1+(1.0f-t)*(float)r2;
	uint8_t g3 = t*(float)g1+(1.0f-t)*(float)g2;
	uint8_t b3 = t*(float)b1+(1.0f-t)*(float)b2;
	uint8_t a3 = t*(float)a1+(1.0f-t)*(float)a2;
	return (a3<<24) | (b3<<16) | (g3<<8) | r3;
}

int main()
{
	sf2d_init();
	sf2d_set_vblank_wait(0);

	// this will depend on the level
	uint8_t mapWidth = 4;
	uint8_t mapHeight = 3;
	uint16_t mapDim = mapWidth*mapHeight;
	uint8_t map_u[mapDim];
	uint16_t mapLength = generateLevel(map_u, mapWidth, mapHeight, 12);
	uint8_t map[mapDim];
	memcpy(map, map_u, mapDim);

	// get some drawing parameters
	uint16_t areaWidth = TWIDTH*2/3;
	uint16_t areaHeight = THEIGHT*2/3;
	uint16_t recWidth = areaWidth/mapWidth;
	uint16_t recHeight = areaHeight/mapHeight;
	uint16_t areaTop = (THEIGHT-mapHeight*recHeight)/2;
	uint16_t areaLeft = (TWIDTH-mapWidth*recWidth)/2;

	uint8_t curX = 0;
	uint8_t curY = 0;
	uint16_t playerLength = 0;
	float colorInterpolation = 0;

	u64 oldTime = 0;
	u64 keyTime = 0;

	while (aptMainLoop()) {
		// manage timer according to time passed since last frame
		u64 newTime = osGetTime();
		keyTime += newTime-oldTime;
		oldTime = newTime;

		hidScanInput();
		if (hidKeysDown() & KEY_START) break;

		// move cursor according to input
		uint16_t oldCurXY = curY*mapWidth+curX;
		curX += mapWidth;
		curY += mapHeight;
		if (hidKeysDown() & KEY_LEFT) curX--;
		if (hidKeysDown() & KEY_RIGHT) curX++;
		if (hidKeysDown() & KEY_UP) curY--;
		if (hidKeysDown() & KEY_DOWN) curY++;
		curX %= mapWidth;
		curY %= mapHeight;
		uint16_t newCurXY = curY*mapWidth+curX;

		if (newCurXY != oldCurXY) {
			map[newCurXY]--;
			playerLength++;
			keyTime=0; // force cursor display now
		}
		if (map[newCurXY] == 255) {
			// reset level
			curX = 0;
			curY = 0;
			playerLength = 0;
			memcpy(map, map_u, mapDim);
		}
		if (playerLength == mapLength) break; // TODO should be "you won"

		u32 targetColor = RGBA8(0x00,0xaa,0xaa,255);
		float targetInterpolation = (float)playerLength/(float)mapLength;
		if (newTime%256==0) colorInterpolation = (colorInterpolation*3+targetInterpolation*1)/4;
		u32 darkColor = interpolate(targetColor, greyed(targetColor), colorInterpolation);
		u32 liteColor = interpolate(darkColor, 0xffffffff, 1.0f/3.0f); // white is too clear
		u32 bgColor = interpolate(darkColor, 0, 0.5f);

		sf2d_set_clear_color(bgColor);

		sf2d_start_frame(GFX_TOP, GFX_LEFT);
		{
			// draw tiles
			for (uint8_t x=0; x<mapWidth; x++) for (uint8_t y=0; y<mapHeight; y++) {
				sf2d_draw_rectangle(x*recWidth+areaLeft, y*recHeight+areaTop, recWidth, recHeight, interpolate(darkColor, liteColor, ((float)map[y*mapWidth+x])/3));
			}

			// draw cursor
			float t = (float)(keyTime%2048)/2048.0f;
			uint8_t op = pow(fabs(t-0.5f)*2,4)*255;
			sf2d_draw_rectangle(curX*recWidth+areaLeft, curY*recHeight+areaTop, recWidth, recHeight, RGBA8(0,0,0,op));
		}
		sf2d_end_frame();

		sf2d_swapbuffers();
	}

	sf2d_fini();

	return 0;
}
