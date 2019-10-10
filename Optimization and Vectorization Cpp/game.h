// Template, major revision 3, beta
// IGAD/NHTV - Jacco Bikker - 2006-2009

#pragma once
#define STARS		19000
#define MAXACTORS	1000
#define SLICES	32 //always to be a power of 2 (tested at 32, created artifacts
#define SLICEDIVISION 5 //the power of 2 for SLICES

namespace Tmpl8 {

class Surface;
class Sprite;

class Actor
{
public:
	enum
	{
		UNDEFINED = 0,
		METALBALL = 1,
		PLAYER = 2,
		ENEMY = 3,
		BULLET = 4
	};
	virtual bool Tick() = 0;
	virtual bool Hit( float& a_X, float& a_Y, float& a_NX, float& a_NY ) { return false; }
	virtual int GetType() { return Actor::UNDEFINED; }
	static void SetSurface( Surface* a_Surface ) { m_Surface = a_Surface; }
	static Surface* m_Surface;
	Sprite* m_Sprite;
	static Sprite* m_Spark;
	float m_X, m_Y;
};

class MetalBall;
class ActorPool
{
public:
	ActorPool() { m_Pool = new Actor*[MAXACTORS]; m_Actors = 0; }
	static void Tick() 
	{ 
		for ( int i = 0; i < m_Actors; i++ ) 
		{
			Actor* actor = m_Pool[i];
			if (!actor->Tick()) delete actor;
		}
	}
	static void Add( Actor* a_Actor ) { m_Pool[m_Actors++] = a_Actor; }
	static void Delete( Actor* a_Actor )
	{
		for ( int i = 0; i < m_Actors; i++ ) if (m_Pool[i] == a_Actor)
		{
			for ( int j = i + 1; j < m_Actors; j++ ) m_Pool[j - 1] = m_Pool[j];
			m_Actors--;
			break;
		}
	}
	static bool CheckHit( float& a_X, float& a_Y, float& a_NX, float& a_NY )
	{
		for ( int i = 0; i < m_Actors; i++ )
			if (m_Pool[i]->Hit( a_X, a_Y, a_NX, a_NY )) return true;
		return false;
	}
	static int GetActiveActors() { return m_Actors; }
	static Actor** m_Pool;
	static int m_Actors;
};

class Surface;
class Starfield : public Actor
{
public:
	Starfield();
	bool Tick();
private:
	float* x, *y;
};

class Bullet : public Actor
{
public:
	Bullet();
	~Bullet();
	enum
	{
		PLAYER = 0,
		ENEMY = 1
	};
	void Init( Surface* a_Surface, float a_X, float a_Y, float a_VX, float a_VY, int a_Owner )
	{
		m_X = a_X, m_Y = a_Y;
		m_VX = a_VX, m_VY = a_VY;
		m_Surface = a_Surface;
		m_Life = 1200;
		m_Owner = a_Owner;
	}
	bool Tick();
	int GetType() { return Actor::BULLET; }
	float m_VX, m_VY;
	int m_Life, m_Owner;
	Sprite* m_Player, *m_Enemy;
};

class MetalBall : public Actor
{
public:
	MetalBall();
	bool Tick();
	bool Hit( float& a_X, float& a_Y, float& a_NX, float& a_NY );
	int GetType() { return Actor::METALBALL; }
};

class Playership : public Actor
{
public:
	Playership();
	bool Tick();
	int GetType() { return Actor::PLAYER; }
private:
	float m_VX, m_VY;
	int m_BTimer, m_DTimer;
	Sprite* m_Death;
};

class Enemy : public Actor
{
public:
	Enemy();
	bool Tick();
	int GetType() { return Actor::ENEMY; }
private:
	float m_VX, m_VY;
	int m_Frame, m_BTimer, m_DTimer;
	Sprite* m_Death;
};

class Game
{
public:
	void SetTarget( Surface* a_Surface ) { m_Screen = a_Surface; }
	void Init();
	void Tick( float a_DT );
	void DrawBackdrop();
	void HandleKeys();
	void KeyDown( unsigned int code ) {}
	void KeyUp( unsigned int code ) {}
	void MouseMove( unsigned int x, unsigned int y ) {}
	void MouseUp( unsigned int button ) {}
	void MouseDown( unsigned int button ) {}
private:
	Surface* m_Screen;
	Sprite* m_Ship;
	ActorPool* m_ActorPool;
	int m_Timer;
};

}; // namespace Tmpl8