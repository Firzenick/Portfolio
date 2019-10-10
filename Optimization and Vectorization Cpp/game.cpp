#include "precomp.h"

using namespace Tmpl8;

Actor **ActorPool::m_Pool;
int ActorPool::m_Actors;
ActorPool actorpool;
Surface *Actor::m_Surface;
Sprite *Actor::m_Spark;
Surface *backdrop = new Surface( "assets/backdrop.png" );
int slices[SLICES][MAXACTORS]; //would do grid, but DrawBackdrop ignores only based on x, so only separate on x
int perSlice[SLICES];		   //a counter for how many Actors end up in each slice

Starfield::Starfield()
{
	x = new float[STARS], y = new float[STARS];
	for ( short i = 0; i < STARS; i++ ) x[i] = Rand( SCRWIDTH ), y[i] = Rand( SCRHEIGHT - 2 );
}

bool Starfield::Tick()
{
	for ( short i = 0; i < STARS; i++ )
	{
		if ( ( x[i] -= ( (float)i + 1 ) / STARS ) < 0 ) x[i] = SCRWIDTH;
		int color = 15 + (int)( ( (float)i / STARS ) * 200.0 );
		m_Surface->AddPlot( x[i], y[i], color + ( color << 8 ) + ( color << 16 ) );
		if ( ( i & 15 ) == 0 )
		{
			for ( int j = 0; j < 8; j++ )
			{
				int color = 15 + (int)( ( (float)i / STARS ) * ( 160.0 - 20.0 * j ) );
				m_Surface->AddPlot( x[i] + j, y[i], color + ( color << 8 ) + ( color << 16 ) );
			}
		}
	}
	return true;
}

MetalBall::MetalBall()
{
	m_Sprite = new Sprite( new Surface( "assets/ball.png" ), 1 );
	while ( 1 )
	{
		m_X = Rand( SCRWIDTH * 4 ) + SCRWIDTH * 1.2f, m_Y = 10 + Rand( SCRHEIGHT - 70 );
		bool hit = false;
		for ( int i = 2; i < ActorPool::m_Actors; i++ )
		{
			MetalBall *b2 = (MetalBall *)ActorPool::m_Pool[i];
			double dx = b2->m_X - m_X, dy = b2->m_Y - m_Y;
			if ( sqrtf( dx * dx + dy * dy ) < 100 ) hit = true;
		}
		if ( !hit ) break;
	}
}

bool MetalBall::Tick()
{
	if ( ( m_X -= .2f ) < -50 ) m_X = SCRWIDTH * 4;
	m_Sprite->Draw( m_Surface, (int)m_X, (int)m_Y );
	for ( char x = 0; x < 50; x++ )
		for ( char y = 0; y < 50; y++ )
		{
			float tx = m_X + x, ty = m_Y + y;
			if ( ( tx < 0 ) || ( ty < 0 ) || ( tx >= SCRWIDTH ) || ( ty >= SCRHEIGHT ) ) continue;
			Pixel *src1 = m_Sprite->GetBuffer() + x + y * 50;
			if ( !( *src1 & 0xffffff ) ) continue;
			double dx = (double)( x - 25 ) / 25, dy = (double)( y - 25 ) / 25;
			double l = sqrtf( dx * dx + dy * dy ) * .2f * PI;
			short sx = (int)( ( ( m_X + 25 ) + (int)( ( 160 * sin( l ) + 100 ) * dx ) + SCRWIDTH ) ) % SCRWIDTH;
			short sy = (int)( ( ( m_Y + 25 ) + (int)( ( 160 * sin( l ) + 100 ) * dy ) + SCRHEIGHT ) ) % SCRHEIGHT;
			Pixel *src2 = m_Surface->GetBuffer() + sx + sy * m_Surface->GetPitch();
			Pixel *dst = m_Surface->GetBuffer() + (int)tx + (int)ty * m_Surface->GetPitch();
			*dst = AddBlend( *src1, *src2 & 0xffff00 );
		}
	return true;
}

bool MetalBall::Hit( float &a_X, float &a_Y, float &a_NX, float &a_NY )
{
	double dist, dx = a_X - ( m_X + 25 ), dy = a_Y - ( m_Y + 25 );
	if ( ( dist = sqrtf( dx * dx + dy * dy ) ) > 25 ) return false;
	a_NX = (float)( dx / sqrtf( dx * dx + dy * dy ) ), a_NY = (float)( dy / sqrtf( dx * dx + dy * dy ) );
	a_X = (float)( m_X + 25 + ( dx / sqrtf( dx * dx + dy * dy ) ) * 25.5 ), a_Y = (float)( m_Y + 25 + ( dy / sqrtf( dx * dx + dy * dy ) ) * 25.5 );
	return true;
}

Playership::Playership()
{
	m_Sprite = new Sprite( new Surface( "assets/playership.png" ), 9 );
	m_Death = new Sprite( new Surface( "assets/death.png" ), 10 );
	m_X = 10, m_Y = 300, m_VX = m_VY = 0, m_BTimer = 5, m_DTimer = 0;
}

bool Playership::Tick()
{
	int hor = 0, ver = 0;
	if ( m_DTimer )
	{
		m_Death->SetFrame( 9 - ( m_DTimer >> 4 ) );
		m_Death->Draw( m_Surface, (int)m_X - 25, (int)m_Y - 20 );
		if ( !--m_DTimer ) m_X = 10, m_Y = 300, m_VX = m_VY = 0;
		return true;
	}
	if ( m_BTimer ) m_BTimer--;
	if ( GetAsyncKeyState( VK_UP ) )
		m_VY = -1.7f, ver = 3;
	else if ( GetAsyncKeyState( VK_DOWN ) )
		m_VY = 1.7f, ver = 6;
	else
	{
		m_VY *= .97f, m_VY = ( fabs( m_VY ) < .05f ) ? 0 : m_VY;
	}
	if ( GetAsyncKeyState( VK_LEFT ) )
		m_VX = -1.3f, hor = 0;
	else if ( GetAsyncKeyState( VK_RIGHT ) )
		m_VX = 1.3f, hor = 1;
	else
	{
		m_VX *= .97f, m_VX = ( fabs( m_VX ) < .05f ) ? 0 : m_VX;
	}
	m_X = max( 4.0f, min( SCRWIDTH - 140.0f, m_X + m_VX ) );
	m_Y = max( 4.0f, min( SCRHEIGHT - 40.0f, m_Y + m_VY ) );
	m_Sprite->SetFrame( 2 - hor + ver );
	m_Sprite->Draw( m_Surface, (int)m_X, (int)m_Y );
	for ( unsigned short i = 2; i < ActorPool::m_Actors; i++ )
	{
		Actor *a = ActorPool::m_Pool[i];
		if ( a->GetType() == Actor::BULLET )
			if ( ( (Bullet *)a )->m_Owner == Bullet::ENEMY )
			{
				double dx = a->m_X - ( m_X + 20 ), dy = a->m_Y - ( m_Y + 12 );
				if ( sqrtf( dx * dx + dy * dy ) < 15 ) m_DTimer = 159;
			}
		if ( a->GetType() == Actor::ENEMY )
		{
			double dx = ( a->m_X + 16 ) - ( m_X + 20 ), dy = ( a->m_Y + 10 ) - ( m_Y + 12 );
			if ( sqrtf( dx * dx + dy * dy ) < 18 ) m_DTimer = 159;
		}
		if ( a->GetType() != Actor::METALBALL ) continue;
		double dx = ( a->m_X + 25 ) - ( m_X + 20 ), dy = ( a->m_Y + 25 ) - ( m_Y + 12 );
		if ( sqrtf( dx * dx + dy * dy ) < 35 ) m_DTimer = 159;
	}
	if ( ( !GetAsyncKeyState( VK_CONTROL ) ) || ( m_BTimer > 0 ) ) return true;
	Bullet *newbullet = new Bullet();
	newbullet->Init( m_Surface, m_X + 20, m_Y + 18, 1, 0, Bullet::PLAYER );
	ActorPool::Add( newbullet );
	m_BTimer = 8;
	return true;
}

Enemy::Enemy()
{
	m_Sprite = new Sprite( new Surface( "assets/enemy.png" ), 4 );
	m_Death = new Sprite( new Surface( "assets/edeath.png" ), 4 );
	m_VX = -1.4f, m_X = SCRWIDTH * 2 + Rand( SCRWIDTH * 4 );
	m_VY = 0, m_Y = SCRHEIGHT * .2f + Rand( SCRWIDTH * .6f );
	m_Frame = 0, m_BTimer = 5, m_DTimer = 0;
}

bool Enemy::Tick()
{
	if ( m_DTimer )
	{
		m_Death->SetFrame( 3 - ( m_DTimer >> 3 ) );
		m_Death->Draw( m_Surface, (int)m_X - 1015, (int)m_Y - 15 );
		if ( !--m_DTimer ) m_X = SCRWIDTH * 3, m_Y = Rand( SCRHEIGHT ), m_VX = -1.4f, m_VY = 0;
		return true;
	}
	m_X += m_VX, m_Y += ( m_VY *= .99f ), m_Frame = ( m_Frame + 1 ) % 31;
	if ( m_X < -50 ) m_X = SCRWIDTH * 4;
	m_Sprite->SetFrame( m_Frame >> 3 );
	m_Sprite->Draw( m_Surface, (int)m_X, (int)m_Y );
	for ( int i = 0; i < ActorPool::m_Actors; i++ )
	{
		Actor *a = ActorPool::m_Pool[i];
		if ( a->GetType() == Actor::BULLET )
			if ( ( (Bullet *)a )->m_Owner == Bullet::PLAYER )
			{
				double dx = ( m_X + 15 ) - a->m_X, dy = ( m_Y + 11 ) - a->m_Y;
				if ( ( dx * dx + dy * dy ) < 100 )
				{
					m_DTimer = 31, m_X += 1000;
					ActorPool::Delete( a );
					delete a;
				}
			}
		double hdist = ( m_X + 15 ) - ( a->m_X + 25 ), vdist = ( a->m_Y + 25 ) - ( m_Y + 11 );
		if ( ( ( hdist < 0 ) || ( hdist > 120 ) ) || ( a->GetType() != Actor::METALBALL ) ) continue;
		if ( ( vdist > 0 ) && ( vdist < 30 ) ) m_VY -= (float)( ( 121 - hdist ) * .0015 );
		if ( ( vdist < 0 ) && ( vdist > -30 ) ) m_VY += (float)( ( 121 - hdist ) * .0015 );
	}
	if ( m_Y < 100 )
		m_VY += .05f;
	else if ( m_Y > ( SCRHEIGHT - 100 ) )
		m_VY -= .05f;
	Playership *p = (Playership *)ActorPool::m_Pool[1];
	double dx = p->m_X - m_X, dy = p->m_Y - m_Y, dist = sqrtf( dx * dx + dy * dy );
	if ( ( dist > 180 ) || ( dist < 100 ) ) return true;
	m_VX += (float)( ( dx / 50.0 ) / dist ), m_VY += (float)( ( dy / 50.0 ) / dist );
	if ( --m_BTimer )
		return true;
	else
		m_BTimer = 19;
	Bullet *newbullet = new Bullet();
	newbullet->Init( m_Surface, m_X + 15, m_Y + 10, (float)( ( dx / 5.0f ) / dist ), (float)( ( dy / 5.0f ) / dist ), Bullet::ENEMY );
	ActorPool::Add( newbullet );
	return true;
}

Bullet::Bullet()
{
	m_Player = new Sprite( new Surface( "assets/playerbullet.png" ), 1 );
	m_Enemy = new Sprite( new Surface( "assets/enemybullet.png" ), 1 );
}

Bullet::~Bullet()
{
	delete m_Player;
	delete m_Enemy;
}

bool Bullet::Tick()
{
	for ( int i = 0; i < 8; i++ )
	{
		m_X += 1.6f * m_VX, m_Y += 1.6f * m_VY;
		int color = 127 + 32 * i;
		if ( m_Owner == Bullet::PLAYER ) m_Player->Draw( m_Surface, (int)m_X, (int)m_Y );
		if ( m_Owner == Bullet::ENEMY ) m_Enemy->Draw( m_Surface, (int)m_X, (int)m_Y );
		if ( ( !--m_Life ) || ( m_X > SCRWIDTH ) || ( m_X < 0 ) || ( m_Y < 0 ) || ( m_Y > SCRHEIGHT ) )
		{
			ActorPool::Delete( this );
			return false;
		}
		float nx, ny, vx = m_VX, vy = m_VY;
		if ( !ActorPool::CheckHit( m_X, m_Y, nx, ny ) ) continue;
		m_Spark->Draw( m_Surface, (int)m_X - 4, (int)m_Y - 4 );
		m_X += ( m_VX = -2 * ( nx * vx + ny * vy ) * nx + vx );
		m_Y += ( m_VY = -2 * ( nx * vx + ny * vy ) * ny + vy );
	}
	return true;
}

void Game::Init()
{
	ActorPool::Add( new Starfield() );
	ActorPool::Add( new Playership() );
	for ( char i = 0; i < 50; i++ ) ActorPool::Add( new MetalBall() );
	for ( char i = 0; i < 20; i++ ) ActorPool::Add( new Enemy() );
	Actor::SetSurface( m_Screen );
	Actor::m_Spark = new Sprite( new Surface( "assets/hit.png" ), 1 );
	Actor::m_Spark->SetFlags( Sprite::FLARE );

	//fill slices and perSlice with 0s
	for ( int i = 0; i < SLICES; i++ )
	{
		perSlice[i] = 0;
		for ( int j = 0; j < MAXACTORS; j++ )
		{
			slices[i][j] = 0;
		}
	}
}

//original version; in case I break it too much
//void Game::DrawBackdrop()
//{
//	for ( int i, x = 0; x < SCRWIDTH; x += 2 ) for ( int y = 0; y < SCRHEIGHT; y += 2 ) //looping over all even x and y positions, loop instead over only those close to: Playership, Ball and then Enemies
//	{
//		float sum1 = 0, sum2 = 0;
//		for ( i = 1; i < ActorPool::m_Actors; i++ )
//		{
//			Actor* a = ActorPool::m_Pool[i]; //lots of cache misses
//			if (a->GetType() == Actor::ENEMY) break; else if (a->m_X > (x + 120)) continue; //19%, lots of cache misses or function calls
//			double dx = (a->m_X + 20) - x, dy = (a->m_Y + 20) - y;
//			if (abs( dx ) > 0 && abs( dy ) > 0) sum1 += 100000.0 / (float)(dx * dx + dy * dy); //reciprocal sqrt
//		}
//		for (; i < ActorPool::m_Actors; i++ )
//		{
//			Actor* a = ActorPool::m_Pool[i]; //lots of cache misses
//			if (a->GetType() == Actor::BULLET) break; else if (a->m_X > (x + 80)) continue; //9%, lots of cache misses
//			double dx = (a->m_X + 15) - x, dy = (a->m_Y + 12) - y;
//			if (abs( dx ) > 0 && abs( dy ) > 0) sum2 += 70000.0 / (float)(dx * dx + dy * dy); //reciprocal sqrt
//		}
//		int color = (int)min( 255.0f, sum1 ) + ((int)min( 255.0f, sum2 ) << 16), p = m_Screen->GetPitch();
//		m_Screen->GetBuffer()[x + y * p] =
//			AddBlend( color, m_Screen->GetBuffer()[x + y * p] ); //5.94 seconds
//	}
//}

void Game::DrawBackdrop() //current version in development
{
	int p = m_Screen->GetPitch();
	for ( int x = 0; x < SCRWIDTH; x += 2 )
	{
		int cSlice = x >> SLICEDIVISION;
		if ( perSlice[cSlice] > 0 )
		{
			for ( int y = 0; y < SCRHEIGHT; y += 2 )
			{
				float sum1 = 0, sum2 = 0;
				for ( int j = 0; j < perSlice[cSlice]; j++ )
				{
					Actor *a = ActorPool::m_Pool[slices[cSlice][j]];
					if ( a->GetType() != Actor::ENEMY )
					{
						if ( a->m_X > ( x + 120 ) ) continue;
						double dx = ( a->m_X + 20 ) - x, dy = ( a->m_Y + 20 ) - y;
						if ( abs( dx ) > 0 && abs( dy ) > 0 ) sum1 += 100000.0 / (float)( dx * dx + dy * dy );
					}
					else
					{
						if ( a->m_X > ( x + 80 ) ) continue;
						double dx = ( a->m_X + 15 ) - x, dy = ( a->m_Y + 12 ) - y;
						if ( abs( dx ) > 0 && abs( dy ) > 0 ) sum2 += 70000.0 / (float)( dx * dx + dy * dy );
					}
				}
				int color = (int)min( 255.0f, sum1 ) + ( (int)min( 255.0f, sum2 ) << 16 );
				m_Screen->GetBuffer()[x + y * p] = AddBlend( color, m_Screen->GetBuffer()[x + y * p] );
			}
		}
	}
}

//"Blind Stupidity" version Draw only around Player, Balls and Enemies, kept crashing, couldn't figure out why

//void Game::DrawBackdrop()
//{
//	int /*EnemyPoint, BulletPoint,*/ p = m_Screen->GetPitch();
//	int radius = 100;
//	/*for (int i = 1; i< ActorPool::m_Actors; i++){
//		Actor* a = ActorPool::m_Pool[i];
//		if (a->GetType() == Actor::ENEMY) EnemyPoint = i;
//		if (a->GetType() == Actor::BULLET) {BulletPoint = i;break;}
//	}*/
//	int i;
//	for ( i = 1; i < ActorPool::m_Actors; i++ )
//	{
//		Actor* a  = ActorPool::m_Pool[i];
//		if ( a->GetType() == Actor::ENEMY ) break;
//		if(a->m_X < 0 || a->m_X > SCRWIDTH + 120) continue;
//		int min_X = a->m_X - radius;
//		if ( min_X % 2 == 1 ) { min_X++; };
//		int min_Y = a->m_Y - radius;
//		if ( min_Y % 2 == 1 ) { min_Y++; };
//		for ( int y = min_Y; y < min_Y + radius; y += 2 )
//		{
//			if ( y > SCRHEIGHT ) { break; }
//			else
//				for (int x = min_X; x < min_X + radius; x += 2) {
//					int dx = ( a->m_X + 20 ) - x, dy = ( a->m_Y + 20 ) - y;
//					if ( dx != 0 && dy != 0 ) {
//						float sum1 = 100000.0f / (float)( dx * dx + dy * dy );
//						int color = int(min( 255.0f, sum1 ));
//						m_Screen->GetBuffer()[x + y * p] = AddBlend( color, m_Screen->GetBuffer()[x + y * p] );
//					}
//				}
//		}
//	}
//	for ( ; i < ActorPool::m_Actors; i++ )
//	{
//		Actor* a = ActorPool::m_Pool[i];
//		if ( a->GetType() == Actor::BULLET ) break;
//		if ( a->m_X < 30 || a->m_X > SCRWIDTH - 120 ) continue;
//		int min_X = a->m_X - radius;
//		if ( min_X % 2 == 1 ) { min_X++; };
//		int min_Y = a->m_Y - radius;
//		if ( min_Y % 2 == 1 ) { min_Y++; };
//		for ( int y = min_Y; y < min_Y + radius; y += 2 )
//		{
//			if ( y > SCRHEIGHT ) { break; }
//			else
//				for ( int x = min_X; x < min_X + radius; y += 2 )
//				{
//					int dx = ( a->m_X + 15 ) - x, dy = ( a->m_Y + 12 ) - y;
//					if ( dx != 0 && dy != 0 )
//					{
//						float sum2 = 70000.0f / (float)( dx * dx + dy * dy );
//						int color = int( min( 255.0f, sum2 ))<<16;
//						m_Screen->GetBuffer()[x + y * p] = AddBlend( color, m_Screen->GetBuffer()[x + y * p] );
//					}
//				}
//		}
//	}
//}

void Game::Tick( float a_DT )
{
	timer t;
	t.reset();
	m_Screen->Clear( 0 );
	backdrop->CopyTo( m_Screen, 0, 0 );
	//first clear Slices from last frame
	for ( int i = 0; i < SLICES; i++ )
	{
		perSlice[i] = 0; // if this works correctly, the line below should be unnecessary, but I'm a programmer
		slices[i][0] = 0; //only the first one need be overwritten just in case it is accidentally read
	}
	int slice;
	for ( int i = 1; i < ActorPool::m_Actors; i++ )
	{
		Actor *a = ActorPool::m_Pool[i];
		if ( a->GetType() != Actor::UNDEFINED && a->GetType() != Actor::BULLET ) //only Player, Metalball and Enemy to be divided for DrawBackdrop, as only they have any influence
		{
			if ( a->m_X < 0 ) { slice = 0; }
			else{slice = (int)( a->m_X ) >> SLICEDIVISION;} //which slice does the Actor belong to?
			if ( slice >= SLICES ) { continue; } //too far right, ignore, can't do this with too far left as we set that to 0, but most things too far left cease to exist anyway
			//all actors should also be placed to their left and right, in case of being close to the threshold, but keep special cases in mind
			//special cases rather here than x*y times in DrawBackdrop
			if (slice == 0) { //only this and 1 to the right if all the way on the left
				slices[slice][perSlice[slice]] = i;
				perSlice[slice]++;
				slices[slice+1][perSlice[slice+1]] = i;
				perSlice[slice + 1]++;
				slices[slice + 2][perSlice[slice + 2]] = i;
				perSlice[slice + 2]++;
			}
			else if ( slice == SLICES - 2 ) //this and 1 to the left if all the way on the right
			{
				slices[slice][perSlice[slice]] = i;
				perSlice[slice]++;
				slices[slice - 1][perSlice[slice - 1]] = i;
				perSlice[slice - 1]++;
				slices[slice + 1][perSlice[slice + 1]] = i;
				perSlice[slice + 1]++;
			}
			else if ( slice == SLICES - 1 ) //this and 1 to the left if all the way on the right
			{
				slices[slice][perSlice[slice]] = i;
				perSlice[slice]++;
				slices[slice - 1][perSlice[slice - 1]] = i;
				perSlice[slice - 1]++;
			}
			else //this slice and both slices left and right of this slice
			{
				slices[slice][perSlice[slice]] = i;
				perSlice[slice]++;
				slices[slice + 1][perSlice[slice + 1]] = i;
				perSlice[slice + 1]++;

				slices[slice + 2][perSlice[slice + 2]] = i;
				perSlice[slice + 2]++;
				slices[slice - 1][perSlice[slice - 1]] = i;
				perSlice[slice - 1]++;
			}
		}
	}

	DrawBackdrop();
	ActorPool::Tick();
	float elapsed = t.elapsed();
	m_Screen->Box( 2, 2, 12, 66, 0xffffff );

	//cout << 1000 / elapsed << "\n";
	if ( elapsed >= 10 )
	{
		m_Screen->Bar( 4, 4, 10, 64, 0xff0000 );
		return;
	}
	Sleep( 10 - elapsed ); // aim for 100fps
	m_Screen->Bar( 4, ( 10 - elapsed ) * 6 + 4, 10, 64, 0x00ff00 );
}