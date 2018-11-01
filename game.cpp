#include "precomp.h"
#include <vector>
// global data (source scope)
static Game* game;
static Font font( "assets/digital_small.png", "ABCDEFGHIJKLMNOPQRSTUVWXYZ:?!=-0123456789." );
static bool lock = false;
static timer stopwatch;
static float duration;

// mountain peaks (push player away)
//static float peakx[16] = { 496, 1074, 1390, 1734, 1774, 426, 752, 960, 1366, 1968, 728, 154, 170, 1044, 828, 1712 };
//static float peaky[16] = { 398, 446, 166, 748, 1388, 1278, 938, 736, 1090, 290, 126, 82, 784, 570, 894, 704 };
//static float peakh[16] = { 400, 300, 320, 510, 400, 510, 400, 600, 240, 200, 160, 160, 160, 320, 320, 320 };
static int peakx[16] = { 496, 1074, 1390, 1734, 1774, 426, 752, 960, 1366, 1968, 728, 154, 170, 1044, 828, 1712 };
static int peaky[16] = { 398, 446, 166, 748, 1388, 1278, 938, 736, 1090, 290, 126, 82, 784, 570, 894, 704 };
static int peakh[16] = { 400, 300, 320, 510, 400, 510, 400, 600, 240, 200, 160, 160, 160, 320, 320, 320 };


// player, bullet and smoke data
static int aliveP1 = MAXP1, aliveP2 = MAXP2, frame = 0;
static Bullet bullet[MAXBULLET];
static GameThread gt;

//Lookup table
static float sinf_dic[720];
static float cosf_dic[720];
static float sqrtf_dic[1500];
static vector<uint> GRID[GRIDCOUNTS];
static vector<uint> GRID_B[GRIDCOUNTS_B];
#define AttackGridRange 2
#define THREADMODE
#define USEBULLETGRID


//buffer

//global objects 

//grid approach 
//static Grid


// dust particle effect tick function
void Particle::Tick()
{
	pos += vel;

	if (pos.x < 0) pos.x = 2046, pos.y = (float)((idx * 20261) % 1534); // adhoc rng
	if (pos.y < 0) pos.y = 1534;
	if (pos.x > 2046) pos.x = 0;
	if (pos.y > 1534) pos.y = 0;
	int x = pos.x;
	int y = pos.y;
	vec2 force = vec2( -1.0f + vel.x, -0.1f + vel.y ).normalized() * speed;
	Pixel* heights = game->heights->GetBuffer();
	int ix = min( 1022, (int)pos.x / 2 ), iy = min( 766, (int)pos.y / 2 );
	float heightDeltaX = (float)(heights[ix + iy >>10] & 255) - (heights[(ix + 1) + iy >>10] & 255);
	float heightDeltaY = (float)(heights[ix + iy >>10] & 255) - (heights[ix + (iy + 1) >>10] & 255);
	vec3 N = normalize( vec3( heightDeltaX, heightDeltaY, 38 ) ) * 4.0f;
	vel.x = force.x + N.x, vel.y = force.y + N.y;
	Pixel* a = game->canvas->GetBuffer() + x + y * 2048;
	/*a[0] = *(game->blend->GetBuffer() + x + y * 2048);
	a[1] = *(game->blend->GetBuffer() + (x+1) + y * 2048);
	a[2048] = *(game->blend->GetBuffer() + x + (y + 1) * 2048);
	a[2049] = *(game->blend->GetBuffer() + (x+1) + (y + 1) * 2048);*/
	a[0] = AddBlend( a[0], 0x221100 ), a[2048] = AddBlend( a[2048], 0x221100 );
	a[1] = AddBlend( a[1], 0x221100 ), a[2049] = AddBlend( a[2049], 0x221100 );
}

// smoke particle effect tick function
void Smoke::Tick()
{
	unsigned int p = frame >> 3;
	if (frame < 64) if (!(frame++ & 7)) puff[p].x = xpos, puff[p].y = ypos << 8, puff[p].vy = -450, puff[p].life = 63;
	for (unsigned int i = 0; i < p; i++) if ((frame < 64) || (i & 1))
	{
		puff[i].x++, puff[i].y += puff[i].vy, puff[i].vy += 3;
		int frame = (puff[i].life > 13) ? (9 - (puff[i].life - 14) / 5) : (puff[i].life / 2);
		game->smoke->SetFrame( frame );
		game->smoke->Draw( puff[i].x - 12, (puff[i].y >> 8) - 12, game->canvas );
		if (!--puff[i].life) puff[i].x = xpos, puff[i].y = ypos << 8, puff[i].vy = -450, puff[i].life = 63;
	}
}

// bullet Tick function
void Bullet::Tick()
{
	if (!(flags & Bullet::ACTIVE)) return;
	vec2 prevpos = pos;
	pos += speed * 1.5f, prevpos -= pos - prevpos;
	game->canvas->AddLine( prevpos.x, prevpos.y, pos.x, pos.y, 0x555555 );
	if ((pos.x < 0) || (pos.x > 2047) || (pos.y < 0) || (pos.y > 1535)) flags = 0; // off-screen
	unsigned int start = 0, end = MAXP1;
	if (flags & P1) start = MAXP1, end = MAXP1 + MAXP2;
	for (unsigned int i = start; i < end; i++) // check all opponents
	{
		Tank& t = game->tankPrev[i];
		if (!((t.flags & Tank::ACTIVE) && (pos.x >( t.pos.x - 2 )) && (pos.y > (t.pos.y - 2)) && (pos.x < (t.pos.x + 2)) && (pos.y < (t.pos.y + 2)))) continue;
		game->tank[i].health = max( 0.0f, game->tank[i].health - (Rand( 0.3f ) + 0.1f) );
		if (game->tank[i].health > 0) continue;
		if (t.flags & Tank::P1) aliveP1--; else aliveP2--;	// update counters
		game->tank[i].flags &= Tank::P1 | Tank::P2;			// kill tank
		flags = 0;											// destroy bullet
		break;
	}
}

void Bullet::TickUpdate()
{
	if (!(flags & Bullet::ACTIVE)) return;
	//vec2 prevpos = pos;
	//pos += speed * 1.5f, prevpos -= pos - prevpos;
	//game->canvas->AddLine(prevpos.x, prevpos.y, pos.x, pos.y, 0x555555);
	if ((pos.x < 0) || (pos.x > 2047) || (pos.y < 0) || (pos.y > 1535)) flags = 0; // off-screen



	#ifdef USEBULLETGRID
	{
		int gx = int(pos.x) >> 5;
		int gy = int(pos.y) >> 5;
		uint baseidx = gx + (gy << 6);
		//if (gx >= GNUMX || gx < 0 || gy >= GNUMY || gy < 0)
		bool r = flags & P1;
		int range = AttackGridRange;
		int minGX = max((gx - range), 0);
		int maxGX = min((gx + range), GNUMX);
		int minGY = max((gy - range), 0);
		int maxGY = min((gy + range), GNUMY);
		for (int x = minGX; x < maxGX; x++) for (int y = minGY; y < maxGY; y++)
		{
			uint index = x + y * GNUMX;
			for (auto &tankid : GRID[index])
			{
				if (r && (tankid < MAXP1)) continue; // P1 should shoot P2
				if (!r && (tankid > MAXP1)) continue; //P2 should shoot P1

				Tank& t = game->tankPrev[tankid];
				if (!((t.flags & Tank::ACTIVE) && (pos.x > (t.pos.x - 2)) && (pos.y > (t.pos.y - 2)) && (pos.x < (t.pos.x + 2)) && (pos.y < (t.pos.y + 2)))) continue;
				game->tank[tankid].health = max(0.0f, game->tank[tankid].health - (Rand(0.3f) + 0.1f));
				if (game->tank[tankid].health > 0) continue;
				if (t.flags & Tank::P1) aliveP1--; else aliveP2--;	// update counters
				game->tank[tankid].flags &= Tank::P1 | Tank::P2;			// kill tank
				flags = 0;											// destroy bullet
				break;
			}
		}
	}
	#else
	{
		unsigned int start = 0, end = MAXP1;
		if (flags & P1) start = MAXP1, end = MAXP1 + MAXP2;
		for (unsigned int i = start; i < end; i++) // check all opponents
		{
			Tank& t = game->tankPrev[i];
			if (!((t.flags & Tank::ACTIVE) && (pos.x > (t.pos.x - 2)) && (pos.y > (t.pos.y - 2)) && (pos.x < (t.pos.x + 2)) && (pos.y < (t.pos.y + 2)))) continue;
			game->tank[i].health = max(0.0f, game->tank[i].health - (Rand(0.3f) + 0.1f));
			if (game->tank[i].health > 0) continue;
			if (t.flags & Tank::P1) aliveP1--; else aliveP2--;	// update counters
			game->tank[i].flags &= Tank::P1 | Tank::P2;			// kill tank
			flags = 0;											// destroy bullet
			break;
		}
	}
	#endif

}

// Tank::Fire - spawns a bullet
void Tank::Fire( unsigned int party, vec2& pos, vec2& dir )
{
	for (unsigned int i = 0; i < MAXBULLET; i++) if (!(bullet[i].flags & Bullet::ACTIVE))
	{
		bullet[i].flags |= Bullet::ACTIVE + party; // set owner, set active
		bullet[i].pos = pos, bullet[i].speed = speed;
		break;
	}
}

// Tank::Tick - update single tank
void Tank::Tick()
{
	if (!(flags & ACTIVE)) // dead tank
	{
		smoke.xpos = (int)pos.x, smoke.ypos = (int)pos.y;
		return smoke.Tick();
	}
	vec2 force = ( target - pos ).normalized();
	// evade mountain peaks
	for (unsigned int i = 0; i < 16; i++)
	{
		vec2 d( pos.x - peakx[i], pos.y - peaky[i] );
		float sd = (d.x * d.x + d.y * d.y) * 0.2f;
		if (sd < 1500)
		{
			force += d * 0.03f * ((float)peakh[i] / sd);
			float r = sqrtf_dic[(int)sd];
			for (int j = 0; j < 720; j++)
			{
				int x = peakx[i] + (int)(r * sinf_dic[j]);
				int y = peaky[i] + (int)(r * cosf_dic[j]);
				game->canvas->AddPlot( x, y, 0x000500 );
			}
		}
	}
	int gx = int(pos.x) >> 5;
	int gy = int(pos.y) >> 5;
	//uint baseidx = gx + gy * GNUMX;
	uint baseidx = gx + (gy << 6);
	if (gx >= GNUMX || gx < 0 || gy >= GNUMY || gy < 0)
	{
		//using original algorithm if the position is out-bound
		for (unsigned int i = 0; i < (MAXP1 + MAXP2); i++)
		{
			if (&game->tank[i] == this) continue;
			vec2 d = pos - game->tankPrev[i].pos;
			if (d.length() < 8) force += d.normalized() * 2.0f;
			else if (d.length() < 16) force += d.normalized() * 0.4f;
		}
	}
	else
	{
		// evade other tanks in the same cell (KEY)
		int range = AttackGridRange;
		int minGX = max((gx - range), 0);
		int maxGX = min((gx + range), GNUMX);
		int minGY = max((gy - range), 0);
		int maxGY = min((gy + range), GNUMY);
		for (int x = minGX; x < maxGX; x++) for (int y = minGY; y < maxGY; y++)
		for (auto &tankid : GRID[baseidx])
		{
			if (tankid > (MAXP1 + MAXP2)) printf("error");
			if (&game->tank[tankid] == this) continue;
			vec2 d = pos - game->tankPrev[tankid].pos;
			if (d.length2() < 64) force += d.normalized() * 2.0f;
			else if (d.length2() < 256) force += d.normalized() * 0.4f;
		}
	}
	// evade user dragged line
	if ((flags & P1) && (game->leftButton))
	{
		float x1 = (float)game->dragXStart, y1 = (float)game->dragYStart;
		float x2 = (float)game->mousex, y2 = (float)game->mousey;
		if (x1 != x2 || y1 != y2)
		{
			vec2 N = vec2( y2 - y1, x1 - x2 ).normalized();
			float dist = N.dot( pos ) - N.dot( vec2( x1, y1 ) );
			if (fabs( dist ) < 10) if (dist > 0) force += N * 20; else force -= N * 20;
		}
	}
	// update speed using accumulated force
	speed += force, speed = speed.normalized(), pos += speed * maxspeed * 0.5f;
	
	
	// shoot, if reloading completed
	if (--reloading >= 0) return;
	//Using Grid to check neighboring tanks. 
	//#define USEGRID
	#ifdef USEGRID
		bool r = flags & P1;
		int range = AttackShootGridRange;
		int minGX = max((gx - range), 0);
		int maxGX = min((gx + range), GNUMX);
		int minGY = max((gy - range), 0);
		int maxGY = min((gy + range), GNUMY);
		for (int x = minGX; x < maxGX; x++) for (int y = minGY; y < maxGY; y++)
		{
			uint index = x + y * GNUMX;
			for (auto &tankid : GRID[index])
			{
				if (r && (tankid < MAXP1)) continue; // P1 should shoot P2
				if (!r && (tankid > MAXP1)) continue; //P2 should shoot P1
				vec2 d = game->tankPrev[tankid].pos - pos;
				if (d.length2() < 10000 && speed.dot(d.normalized()) > 0.99999f)
				{
					Fire(flags & (P1 | P2), pos, speed); // shoot
					reloading = 200; // and wait before next shot is ready
					break;
				}
			}
		}
	#else
		unsigned int start = 0, end = MAXP1;
		if (flags & P1) start = MAXP1, end = MAXP1 + MAXP2;
		for (unsigned int i = start; i < end; i++) if (game->tankPrev[i].flags & ACTIVE)
		{
			vec2 d = game->tankPrev[i].pos - pos;
			if (d.length2() < 10000 && speed.dot(d.normalized()) > 0.99999f)
			{
				Fire(flags & (P1 | P2), pos, speed); // shoot
				reloading = 200; // and wait before next shot is ready
				break;
			}
		}
	#endif // USEGRID


	
}

void Tank::TickUpdate()
{
	int gx = int(pos.x) >> 5;
	int gy = int(pos.y) >> 5;
	//uint baseidx = gx + gy * GNUMX;
	uint baseidx = gx + (gy << 6);
	if (gx >= GNUMX || gx < 0 || gy >= GNUMY || gy < 0)
		return;

	if (!(flags & ACTIVE)) // dead tank
	{
		smoke.xpos = (int)pos.x, smoke.ypos = (int)pos.y;
		return smoke.Tick();
	}
	vec2 force = (target - pos).normalized();
	// evade mountain peaks
	for (unsigned int i = 0; i < 16; i++)
	{
		vec2 d(pos.x - peakx[i], pos.y - peaky[i]);
		float sd = (d.x * d.x + d.y * d.y) * 0.2f;
		if (sd < 1500)
		{
			force += d * 0.03f * ((float)(peakh[i] / sd));
			float r = sqrtf_dic[(int)sd];
			for (int j = 0; j < 720; j++)
			{
				int x = peakx[i] + (int)(r * sinf_dic[j]);
				int y = peaky[i] + (int)(r * cosf_dic[j]);
				game->canvas->AddPlot(x, y, 0x000500);
			}
		}
	}

	// evade other tanks in the same cell (KEY)
	int range = AttackGridRange;
	int minGX = max((gx - range), 0);
	int maxGX = min((gx + range), GNUMX);
	int minGY = max((gy - range), 0);
	int maxGY = min((gy + range), GNUMY);
	for (int x = minGX; x < maxGX; x++) for (int y = minGY; y < maxGY; y++)
	for (auto &tankid : GRID[baseidx])
	{
		if (tankid > (MAXP1 + MAXP2)) printf("error");
		if (&game->tank[tankid] == this) continue;
		vec2 d = pos - game->tankPrev[tankid].pos;
		if (d.length2() < 64) force += d.normalized() * 2.0f;
		else if (d.length2() < 256) force += d.normalized() * 0.4f;
	}

	// evade user dragged line
	if ((flags & P1) && (game->leftButton))
	{
		float x1 = (float)game->dragXStart, y1 = (float)game->dragYStart;
		float x2 = (float)game->mousex, y2 = (float)game->mousey;
		if (x1 != x2 || y1 != y2)
		{
			vec2 N = vec2(y2 - y1, x1 - x2).normalized();
			float dist = N.dot(pos) - N.dot(vec2(x1, y1));
			if (fabs(dist) < 10) if (dist > 0) force += N * 20; else force -= N * 20;
		}
	}
	// update speed using accumulated force
	speed += force, speed = speed.normalized(), pos += speed * maxspeed * 0.5f;


	// shoot, if reloading completed
	if (--reloading >= 0) return;
	//Using Grid to check neighboring tanks. 
	bool r = flags & P1;
	//int range = AttackShootGridRange;
	//int minGX = max((gx - range), 0);
	//int maxGX = min((gx + range), GNUMX);
	//int minGY = max((gy - range), 0);
	//int maxGY = min((gy + range), GNUMY);
	for (int x = minGX; x < maxGX; x++) for (int y = minGY; y < maxGY; y++)
	{
		uint index = x + y * GNUMX;
		for (auto &tankid : GRID[index])
		{
			if (r && (tankid < MAXP1)) continue; // P1 should shoot P2
			if (!r && (tankid > MAXP1)) continue; //P2 should shoot P1
			vec2 d = game->tankPrev[tankid].pos - pos;
			if (d.length2() < 10000 && speed.dot(d.normalized()) > 0.99999f)
			{
				Fire(flags & (P1 | P2), pos, speed); // shoot
				reloading = 200; // and wait before next shot is ready
				break;
			}
		}
	}
}

// Game::Init - Load data, setup playfield
void Game::Init()
{
	//LUT for sinf,cosf
	for (int j = 0; j < 720; j++)
	{
		sinf_dic[j] = sinf(j * PI / 360.0f);
		cosf_dic[j] = cosf(j * PI / 360.0f);
	}
	//LUT for sqrtf
	for (int i = 0; i < 1500; i++)
	{
		sqrtf_dic[i] = sqrtf(i);

	}


	// load assets
	backdrop = new Surface( "assets/backdrop.png" );
	heights = new Surface( "assets/heightmap.png" );
	canvas = new Surface( 2048, 1536 ); // game runs on a double-sized surface
	blend = new Surface(2048, 1536);
	tank = new Tank[MAXP1 + MAXP2];
	tankPrev = new Tank[MAXP1 + MAXP2];
	p1Sprite = new Sprite( new Surface( "assets/p1tank.tga" ), 1, Sprite::FLARE );
	p2Sprite = new Sprite( new Surface( "assets/p2tank.tga" ), 1, Sprite::FLARE );
	pXSprite = new Sprite( new Surface( "assets/deadtank.tga" ), 1, Sprite::BLACKFLARE );
	smoke = new Sprite( new Surface( "assets/smoke.tga" ), 10, Sprite::FLARE );
	// create blue tanks
	for (unsigned int i = 0; i < MAXP1; i++)
	{
		Tank& t = tank[i];
		t.pos = vec2( (float)((i % 20) * 20) + P1STARTX, (float)((i / 20) * 20 + 50) + P1STARTY );
		t.target = vec2( 2048, 1536 ); // initially move to bottom right corner
		t.speed = vec2( 0, 0 ), t.flags = Tank::ACTIVE | Tank::P1, t.maxspeed = (i < (MAXP1 / 2)) ? 0.65f : 0.45f;
		t.health = 1.0f;
	}
	// create red tanks
	for (unsigned int i = 0; i < MAXP2; i++)
	{
		Tank& t = tank[i + MAXP1];
		t.pos = vec2( (float)((i % 32) * 20 + P2STARTX), (float)((i / 32) * 20 + P2STARTY) );
		t.target = vec2( 424, 336 ); // move to player base
		t.speed = vec2( 0, 0 ), t.flags = Tank::ACTIVE | Tank::P2, t.maxspeed = 0.3f;
	}
	// create dust particles
	particle = new Particle[DUSTPARTICLES];
	srand( 0 );
	for (int i = 0; i < DUSTPARTICLES; i++)
	{
		particle[i].pos.x = Rand( 2048 );
		particle[i].pos.y = Rand( 1536 );
		particle[i].idx = i;
		particle[i].speed = Rand( 2 ) + 0.5f;
		particle[i].vel.x = particle[i].vel.y = 0;
	}
	game = this; // for global reference
	leftButton = prevButton = false;
#ifdef THREADMODE
	gt.start();
#endif // THREADMODE

	// draw backdrop
	backdrop->CopyTo(blend, 0, 0);
	//canvas->CopyTo(blend, 0, 0);
	for(int x=0;x<2046;x++) for(int y=0;y<1534;y++)
	{
		Pixel* a = game->blend->GetBuffer() + x + y * 2048;
		a[0] = AddBlend(a[0], 0x221100), a[2048] = AddBlend(a[2048], 0x221100);
		a[1] = AddBlend(a[1], 0x221100), a[2049] = AddBlend(a[2049], 0x221100);
	}

}

// Game::DrawDeadTanks - draw dead tanks
void Game::DrawDeadTanks()
{
	for (unsigned int i = 0; i < (MAXP1 + MAXP2); i++)
	{
		Tank& t = tank[i];
		float x = t.pos.x, y = t.pos.y;
		if (!(t.flags & Tank::ACTIVE)) pXSprite->Draw( (int)x - 4, (int)y - 4, canvas );
	}
}

// Game::DrawTanks - draw the tanks
void Game::DrawTanks()
{
	for (unsigned int i = 0; i < (MAXP1 + MAXP2); i++)
	{
		Tank& t = tank[i];
		float x = t.pos.x, y = t.pos.y;
		vec2 p1( x + 70 * t.speed.x + 22 * t.speed.y, y + 70 * t.speed.y - 22 * t.speed.x );
		vec2 p2( x + 70 * t.speed.x - 22 * t.speed.y, y + 70 * t.speed.y + 22 * t.speed.x );
		if (!(t.flags & Tank::ACTIVE)) continue;
		else if (t.flags & Tank::P1) // draw blue tank
		{
			p1Sprite->Draw( (int)x - 4, (int)y - 4, canvas );
			canvas->ThickLine( (int)x, (int)y, (int)(x + 8 * t.speed.x), (int)(y + 8 * t.speed.y), 0x6666ff );
			if (tank[i].health > 0.9f) canvas->ThickLine( (int)x - 4, (int)y - 12, (int)x + 4, (int)y - 12, 0x33ff33 );
			else if (tank[i].health > 0.6f) canvas->ThickLine( (int)x - 4, (int)y - 12, (int)x + 2, (int)y - 12, 0x33ff33 );
			else if (tank[i].health > 0.3f) canvas->ThickLine( (int)x - 4, (int)y - 12, (int)x - 1, (int)y - 12, 0xdddd33 );
			else if (tank[i].health > 0) canvas->ThickLine( (int)x - 4, (int)y - 12, (int)x - 3, (int)y - 12, 0xff3333 );
		}
		else // draw red tank
		{
			p2Sprite->Draw( (int)x - 4, (int)y - 4, canvas );
			canvas->ThickLine( (int)x, (int)y, (int)(x + 8 * t.speed.x), (int)(y + 8 * t.speed.y), 0xff4444 );
		}
		// tread marks
		if ((x >= 0) && (x < 2048) && (y >= 0) && (y < 1536))
			backdrop->GetBuffer()[(int)x + (int)y * 2048] = SubBlend( backdrop->GetBuffer()[(int)x + (int)y * 2048], 0x030303 );
	}
}

// Game::PlayerInput - handle player input
void Game::PlayerInput()
{
	if (leftButton) // drag line to guide player tanks
	{
		if (!prevButton) dragXStart = mousex, dragYStart = mousey, dragFrames = 0; // start line
		canvas->ThickLine( dragXStart, dragYStart, mousex, mousey, 0xffffff );
		dragFrames++;
	}
	else // select a new destination for the player tanks
	{
		if ((prevButton) && (dragFrames < 15)) for (unsigned int i = 0; i < MAXP1; i++) tank[i].target = vec2( (float)mousex, (float)mousey );
		canvas->Line( 0, (float)mousey, 2047, (float)mousey, 0xffffff );
		canvas->Line( (float)mousex, 0, (float)mousex, 1535, 0xffffff );
	}
	prevButton = leftButton;
}

// Game::MeasurementStuff: functionality related to final performance measurement
void Game::MeasurementStuff()
{
#ifdef MEASURE
	char buffer[128];
	if (frame & 64) screen->Bar( 980, 20, 1010, 50, 0xff0000 );
	if (frame >= MAXFRAMES) if (!lock) 
	{
		duration = stopwatch.elapsed();
		lock = true;
	}
	//else frame--;
	if (lock)
	{
		int ms = (int)duration % 1000, sec = ((int)duration / 1000) % 60, min = ((int)duration / 60000);
		sprintf( buffer, "%02i:%02i:%03i", min, sec, ms );
		font.Centre( screen, buffer, 200 );
		sprintf( buffer, "SPEEDUP: %4.1f", REFPERF / duration );
		font.Centre( screen, buffer, 340 );
	}
#endif
}

// Game::Tick - main game loop
void Game::Tick( float a_DT )
{
	// get current mouse position relative to full-size bitmap
	POINT p;
#ifdef MEASURE
	p.x = 512 + (int)(400 * sinf( (float)frame * 0.02f )), p.y = 384;
#else
	GetCursorPos( &p );
	ScreenToClient( FindWindow( NULL, TEMPLATE_VERSION ), &p );
	leftButton = (GetAsyncKeyState( VK_LBUTTON ) != 0);
#endif
	mousex = p.x * 2, mousey = p.y * 2;
#ifdef THREADMODE
#else
	// draw backdrop
	backdrop->CopyTo( canvas, 0, 0 );
	// update dust particles
	for (int i = 0; i < DUSTPARTICLES; i++) particle[i].Tick();
	// draw armies
	DrawDeadTanks();
	DrawTanks();
	// update armies
	memcpy( tankPrev, tank, (MAXP1 + MAXP2) * sizeof( Tank ) );
	//Generate GRID
	game->GenerateGrid();
	if (!lock) for (unsigned int i = 0; i < (MAXP1 + MAXP2); i++) tank[i].Tick();
	// update bullets
	if (!lock) for (unsigned int i = 0; i < MAXBULLET; i++) bullet[i].Tick();
	// scale to window size
	canvas->CopyHalfSize( screen );
#endif
	// handle input
#ifndef MEASURE
	PlayerInput();
#endif

	// draw lens
	for (int x = p.x - 80; x < p.x + 80; x++) for (int y = p.y - 80; y < p.y + 80; y++)
	{
		if (sqrtf( (float)(x - p.x) * (x - p.x) + (y - p.y) * (y - p.y) ) > 80.0f) continue;
		int rx = mousex + x - p.x, ry = mousey + y - p.y;
		if (rx > 0 && ry > 0 && x > 0 && y > 0 && rx < 2048 && x < 1024 && ry < 1536 && y < 768)
			screen->Plot( x, y, canvas->GetBuffer()[rx + ry * 2048] );
	}
	// report on game state
	MeasurementStuff();
	char buffer[128];
#ifdef THREADMODE
	sprintf(buffer, "FRAME: %04i", frame);
#else
	sprintf(buffer, "FRAME: %04i", frame++);
#endif
	font.Print( screen, buffer, 350, 580 );
	if ((aliveP1 > 0) && (aliveP2 > 0))
	{
		sprintf( buffer, "blue army: %03i  red army: %03i", aliveP1, aliveP2 );
		return screen->Print( buffer, 10, 10, 0xffff00 );
	}
	if (aliveP1 == 0)
	{
		sprintf( buffer, "sad, you lose... red left: %i", aliveP2 );
		return screen->Print( buffer, 200, 370, 0xffff00 );
	}
	sprintf( buffer, "nice, you win! blue left: %i", aliveP1 );
	screen->Print( buffer, 200, 370, 0xffff00 );
}
void GameThread::run(){
	printf("Thread is created.");
	while (true)
	{
		game->Update();
	}
}

//Data update & Render update
void Game::Update()
{
	//[Old sequence]
	//backdrop->CopyTo(canvas, 0, 0);
	//for (int i = 0; i < DUSTPARTICLES; i++) particle[i].Tick();
	//DrawDeadTanks();
	//DrawTanks();
	//memcpy( tankPrev, tank, (MAXP1 + MAXP2) * sizeof( Tank ) );
	//if (!lock) for (unsigned int i = 0; i < (MAXP1 + MAXP2); i++) tank[i].Tick();
	//if (!lock) for (unsigned int i = 0; i < MAXBULLET; i++) bullet[i].Tick();
	//if (frame >= 4000)
//		return;
	//Pre-setting
	uint data_update_speed = 10;
	if (frame >= 4000)
		data_update_speed = 0;
	else 
		;
		//return;


	//[ Data update] (Only for bottleneck data update
	for (uint i = 0; i < data_update_speed; i++) {
		memcpy(tankPrev, tank, (MAXP1 + MAXP2) * sizeof(Tank));
		TankTickDataUpdate();
		BulletTickDataUpdate();
	}
	backdrop->CopyTo(canvas, 0, 0);
	ParticleDataUpdate();
	////[Data Update for Draw]
	DeadTankDraw();
	TankDraw();
	memcpy(tankPrev, tank, (MAXP1 + MAXP2) * sizeof(Tank));
	TankTickUpdateDraw();
	//DrawTankUpdateDraw();
	BulletTickUpdateDraw();
	
	//copy background only before drawing
	//backdrop->CopyTo(canvas, 0, 0);
	//[Draw]
	//ParticleDraw();
	//DeadTankDraw();
	//TankDraw();
	
	// scale to window size
	canvas->CopyHalfSize(screen);

		frame += data_update_speed;
}


//Data Update
void Game::TankTickDataUpdate() {
	game->GenerateGrid();
	if (!lock) for (unsigned int i = 0; i < (MAXP1 + MAXP2); i++) tank[i].TickUpdate();
}
void Game::BulletTickDataUpdate() {
	if (!lock) for (unsigned int i = 0; i < MAXBULLET; i++) bullet[i].TickUpdate();
}
void Game::ParticleDataUpdate() {
	for (int i = 0; i < DUSTPARTICLES; i++) particle[i].Tick();
}

//Data Update function sets (For drawing)
void Game::TankTickUpdateDraw() {
	game->GenerateGrid();
	if (!lock) for (unsigned int i = 0; i < (MAXP1 + MAXP2); i++) tank[i].Tick();
}
void Game::DrawTankUpdateDraw() {
	
}
void Game::BulletTickUpdateDraw() {
	if (!lock) for (unsigned int i = 0; i < MAXBULLET; i++) bullet[i].Tick();
}

//Drawing function sets
void Game::ParticleDraw() {

}
void Game::DeadTankDraw() {
	DrawDeadTanks();
}
void Game::TankDraw() {
	DrawTanks();
}

void Game::GenerateBulletGrid()
{
	for (auto& v : GRID_B) {
		v.clear();
	}

	for (unsigned int i = 0; i < (MAXP1 + MAXP2); i++)
	{
		int x = (int)tank[i].pos.x;
		int y = (int)tank[i].pos.y;
		//if (x > 2047 || x < 0 || y > 1535 || y < 0)
		//using double height grid to avoid out bound issue.
		if (x < 0 || y < 0)
		{
			continue;
		}
		int baseidx = (x >> 5) + ((y >> 5) << 6);

		if (GRID_B[baseidx].size() < TANKPERCELL)
		{
			GRID_B[baseidx].push_back(i);
			//totalcounts++;
		}
	}

}

void Game::GenerateGrid()
{
	for (auto& v : GRID) {
		v.clear();
	}

	// Create grid for tanks
	//int totalcounts = 0;
	//int out = 0;
	for (unsigned int i = 0; i < (MAXP1 + MAXP2); i++)
	{
		int x = (int)tank[i].pos.x;
		int y = (int)tank[i].pos.y;
		//if (x > 2047 || x < 0 || y > 1535 || y < 0)
		//using double height grid to avoid out bound issue.
		if ( x < 0 || y < 0)
		{
			continue;
		}
		int baseidx =(x >> 5) + ((y >> 5) << 6);

		if(GRID[baseidx].size() < TANKPERCELL)
		{ 
			GRID[baseidx].push_back(i);
			//totalcounts++;
		}
	}
	//printf("total %d", out);
	//for (auto& i : GRID[30])
	//{
		//printf("grid[30]: %d", i);
	//}
}

