#pragma once

namespace Tmpl8 {

// final measurement defines
#define MEASURE										// ENABLE THIS FOR FINAL PERFORMANCE MEASUREMENT
#define REFPERF			(922+52000+3*60000)				// reference performance Jacco's Razer laptop (i7)
// #define REFPERF		(680+31*1000+10*60*1000)		// uncomment and put your reference time here

// modify these values for testing purposes (used when MEASURE is disabled)
#define MAXP1			20			
#define MAXP2			(4 * MAXP1)	// because the player is smarter than the AI
#define MAXBULLET		200
#define P1STARTX		500
#define P1STARTY		300
#define P2STARTX		1300
#define P2STARTY		700
#define DUSTPARTICLES	20000

// standard values for final measurement, do not modify
#define MAXFRAMES		4000
#ifdef MEASURE
#undef MAXP1
#define MAXP1			750				
#undef MAXP2
#define MAXP2			(4 * MAXP1)
#undef P1STARTX
#define P1STARTX		200
#undef P1STARTY
#define P1STARTY		100
#undef P2STARTX
#define P2STARTX		1300
#undef P2STARTY
#define P2STARTY		700
#undef SCRWIDTH
#define SCRWIDTH		1024
#undef SCRHEIGHT
#define SCRHEIGHT		768
#undef DUSTPARTICLES
#define DUSTPARTICLES	10000
#endif



class Particle
{
public:
	void Tick();
	vec2 pos, vel, speed;
	int idx;
};

class Smoke
{
public:
	struct Puff { int x, y, vy, life; };
	Smoke() : active( false ), frame( 0 ) {};
	void Tick();
	Puff puff[8];
	bool active;
	int frame, xpos, ypos;
};

class Tank
{
public:
	enum { ACTIVE = 1, P1 = 2, P2 = 4 };
	Tank() : pos( vec2( 0, 0 ) ), speed( vec2( 0, 0 ) ), target( vec2( 0, 0 ) ), reloading( 0 ) {};
	void Fire( unsigned int party, vec2& pos, vec2& dir );
	void Tick();
	void TickUpdate();
	vec2 pos, speed, target;
	float maxspeed, health;
	int flags, reloading;
	Smoke smoke;
};

class Bullet
{
public:
	enum { ACTIVE = 1, P1 = 2, P2 = 4 };
	Bullet() : flags( 0 ) {};
	void Tick();
	void TickUpdate();
	vec2 pos, speed;
	int flags;
};

class Surface;
class Surface8;
class Sprite;
//class GameThread;
class GameThread : public Thread {
	//void run() override {
	//	printf("thread is created.");
	//}
public:
	void run();
};


class Game
{
public:
	void SetTarget( Surface* s ) { screen = s; }
	void Init();
	void DrawTanks();
	void DrawDeadTanks();
	void PlayerInput();
	void MeasurementStuff();
	void Tick( float a_DT );
	void print() { printf("test"); }
	void Update();
	//Data Update
	void TankTickDataUpdate(); 
	void BulletTickDataUpdate();
	void ParticleDataUpdate();

	//Data Update for Drawing
	void TankTickUpdateDraw();
	void DrawTankUpdateDraw();
	void BulletTickUpdateDraw();

	//Drawing
	void ParticleDraw();
	void DeadTankDraw();
	void TankDraw();

	//GRID
	void GenerateGrid();

	Surface* screen, *canvas, *backdrop, *heights;
	Sprite* p1Sprite, *p2Sprite, *pXSprite, *smoke;
	int mousex, mousey, dragXStart, dragYStart, dragFrames;
	bool leftButton, prevButton;
	Tank* tank, *tankPrev;
	Particle* particle;

};
#define GNUMX  SCRWIDTH * 2 /32
#define GNUMY SCRHEIGHT * 2 /32
#define GNUMX_B  SCRWIDTH * 2 /128
#define GNUMY_B SCRHEIGHT * 2 /128
#define TANKPERCELL 100
//#define GRIDCOUNTS	GNUMX * GNUMY * (TANKPERCELL + 1)
#define GRIDCOUNTS	GNUMX * GNUMY
#define GRIDCOUNTS_B	GNUMX_B * GNUMY_B
}; // namespace Templ8