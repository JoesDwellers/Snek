// SnakeRemake.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <iostream>
#include <Windows.h>
#include <list>
#include <chrono>
#include <thread>
#include <ctime>
#include "sqlite3.h"
using namespace std;


// Snake will be a linked list where each node is a segment of the snake.
struct sSnakeSegment
{
	int iX;
	int iY;
};


int nScreenWidth = 120;
int nScreenHeight = 30;
const int DIR_UP = 0, DIR_RIGHT = 1, DIR_DOWN = 2, DIR_LEFT = 3;

//Keep track of high score
sqlite3* db;

void InitDatabase()
{
	sqlite3_open("highscores.db", &db);

	const char* sQuery =
		"CREATE TABLE IF NOT EXISTS high_scores ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"name TEXT NOT NULL,"
		"score INTEGER NOT NULL);";

	sqlite3_exec(db, sQuery, NULL, NULL, NULL);
}

// SaveScore: Saves the player's score to the database. If the player's name already exists,
// the score is only updated if the new score is higher than the existing one.
// If the player's name does not exist, a new row is inserted.
// Parameters: wsName - the player's name, iScore - the player's score
void SaveScore(const wchar_t* wsName, int iScore)
{
	char sQuery[256];
	char sName[50];
	wcstombs_s(nullptr, sName, 50, wsName, 50);

	// Check if name already exists
	sqlite3_stmt* stmt;
	char sCheckQuery[256];
	sprintf_s(sCheckQuery, "SELECT score FROM high_scores WHERE name = '%s';", sName);
	sqlite3_prepare_v2(db, sCheckQuery, -1, &stmt, NULL);

	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		// Name exists — only update if new score is higher
		int iExistingScore = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);

		if (iScore > iExistingScore)
		{
			sprintf_s(sQuery, "UPDATE high_scores SET score = %d WHERE name = '%s';", iScore, sName);
			sqlite3_exec(db, sQuery, NULL, NULL, NULL);
		}
	}
	else
	{
		// Name doesn't exist — insert new row
		sqlite3_finalize(stmt);
		sprintf_s(sQuery, "INSERT INTO high_scores (name, score) VALUES ('%s', %d);", sName, iScore);
		sqlite3_exec(db, sQuery, NULL, NULL, NULL);
	}
}

// Displaying high scores, only top player and current user shown, inputs are sanitized to prevent SQL injection
void DisplayScores(wchar_t* screen, int nScreenWidth, int nScreenHeight, int iCurrentScore)
{
	sqlite3_stmt* stmt;
	const char* sQuery = "SELECT name, score FROM high_scores ORDER BY score DESC LIMIT 1;";
	sqlite3_prepare_v2(db, sQuery, -1, &stmt, NULL);

	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		const char* sName = (const char*)sqlite3_column_text(stmt, 0);
		int iTopScore = sqlite3_column_int(stmt, 1);
		wsprintf(&screen[16 * nScreenWidth + 44], L"    BEST: %-10hs %d", sName, iTopScore);
	}
	sqlite3_finalize(stmt);
}

// Death message Game Over Screen
void DrawDeathScreen(wchar_t* screen, int nScreenWidth, wchar_t* wsPlayerName, int iScore)
{
	wsprintf(&screen[11 * nScreenWidth + 44], L"============================");
	wsprintf(&screen[12 * nScreenWidth + 44], L"        GAME  OVER          ");
	wsprintf(&screen[13 * nScreenWidth + 44], L"============================");
	wsprintf(&screen[14 * nScreenWidth + 44], L"         %-20ls", wsPlayerName);
	wsprintf(&screen[15 * nScreenWidth + 49], L"SCORE: %d", iScore);
	wsprintf(&screen[18 * nScreenWidth + 44], L"============================");
	wsprintf(&screen[19 * nScreenWidth + 44], L"   PRESS SPACE TO PLAY AGAIN");
}

// Keep track of players name
void GetPlayerName(wchar_t* wsName, int iMaxLen, wchar_t* screen, int nScreenWidth, HANDLE hConsole)
{
	DWORD dwBytesWritten = 0;
	int iLen = 0;
	wmemset(wsName, 0, iMaxLen);

	while (true)
	{
		// Display prompt
		wsprintf(&screen[14 * nScreenWidth + 40], L"  ENTER NAME: %s_          ", wsName);
		WriteConsoleOutputCharacter(hConsole, screen, nScreenWidth * 30, { 0,0 }, &dwBytesWritten);

		// Wait for a keypress
		for (int k = 0; k < 255; k++)
		{
			if (GetAsyncKeyState(k) & 0x8000)
			{
				Sleep(120); // debounce

				if (k == VK_RETURN && iLen > 0)
					return;

				if (k == VK_BACK && iLen > 0)
				{
					wsName[--iLen] = L'\0';
					break;
				}

				if (k >= 'A' && k <= 'Z' && iLen < iMaxLen - 1)
				{
					wsName[iLen++] = (wchar_t)k;
					wsName[iLen] = L'\0';
					break;
				}
			}
		}
	}
}

//Keep track of players personal best score
int LoadPersonalBest(const wchar_t* wsName)
{
	char sName[50];
	wcstombs_s(nullptr, sName, 50, wsName, 50);

	sqlite3_stmt* stmt;
	char sQuery[256];
	sprintf_s(sQuery, "SELECT score FROM high_scores WHERE name = '%s';", sName);
	sqlite3_prepare_v2(db, sQuery, -1, &stmt, NULL);

	int iBest = 0;
	if (sqlite3_step(stmt) == SQLITE_ROW)
		iBest = sqlite3_column_int(stmt, 0);

	sqlite3_finalize(stmt);
	return iBest;
}

int main()
{
		// Create Screen Buffer
	wchar_t* screen = new wchar_t[nScreenWidth * nScreenHeight];
	for (int i = 0; i < nScreenWidth * nScreenHeight; i++) screen[i] = L'#';
	HANDLE hConsole = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
	SetConsoleActiveScreenBuffer(hConsole);
	DWORD dwBytesWritten = 0;
	srand((unsigned)time(NULL));

	// call highest score function
	InitDatabase();

	// players personal best score
	wchar_t wsPlayerName[20] = L"";
	int iPersonalBest = 0;
	bool bNameEntered = false;


	while (1)
	{

		// // SNAKE // //
		list<sSnakeSegment> snake = { {60, 15}, {61, 15}, {62, 15}, {63, 15}, {64, 15}, {65, 15}, {66, 15}, {67, 15}, {68, 15}, {69, 15} };
		int iFoodX = 30;
		int iFoodY = 10;		
		int iScore = 0;
		int iDirection = 3;
		bool bIsDead = false;
		bool bKeyLeft = false, bKeyRight = false, bKeyLeftOld = false, bKeyRightOld = false;
		bool bScoreSaved = false;
		// // SNAKE // //

		while (!bIsDead)
		{
			// Timing and input
			std::this_thread::sleep_for(std::chrono::milliseconds(120));

			// Get Input
			bKeyRight = (0x8000 & GetAsyncKeyState((unsigned char)('\x27'))) != 0;
			bKeyLeft = (0x8000 & GetAsyncKeyState((unsigned char)('\x25'))) != 0;

			if (bKeyRight && !bKeyRightOld)
			{
				iDirection++;
				if (iDirection == 4) iDirection = 0;
			}
			if (bKeyLeft && !bKeyLeftOld)
			{
				iDirection--;
				if (iDirection == -1) iDirection = 3;
			}

			bKeyRightOld = bKeyRight;
			bKeyLeftOld = bKeyLeft;

			// // Game Logic //  //

			// update snake position
			switch (iDirection)
			{
			case 0: // UP
				snake.push_front({ snake.front().iX, snake.front().iY - 1 });
				break;
			case 1: // RIGHT
				snake.push_front({ snake.front().iX + 1, snake.front().iY });
				break;
			case 2: // DOWn
				snake.push_front({ snake.front().iX, snake.front().iY + 1 });
				break;
			case 3: // LEFT
				snake.push_front({ snake.front().iX - 1, snake.front().iY });
				break;
			}

			// Collision Detection
				// Collision detects snake V world
			if (snake.front().iX < 0 || snake.front().iX >= nScreenWidth)
				bIsDead = true;
			if (snake.front().iY < 3 || snake.front().iY >= nScreenHeight - 1)
				bIsDead = true;

			// Collision detects snake V food
			if (snake.front().iX == iFoodX && snake.front().iY == iFoodY)
			{
				// Increase score once personal best is acheived
				iScore++;
				if (iScore > iPersonalBest)
				{
					iPersonalBest = iScore;
				}

				// Place food somewhere not occupied by the snake
				bool bValidPos = false;
				while (!bValidPos)
				{
					iFoodX = rand() % nScreenWidth;
					iFoodY = (rand() % (nScreenHeight - 4)) + 3;
					bValidPos = true;
					for (auto s : snake)
						if (s.iX == iFoodX && s.iY == iFoodY)
						{
							bValidPos = false;
							break;
						}
				}

				for (int i = 0; i < 5; i++)
					snake.push_back({ snake.back().iX, snake.back().iY });
			}

			// Collision detects snake V self)
			for (list<sSnakeSegment>::iterator i = snake.begin(); i != snake.end(); i++)
				if (i != snake.begin() && i->iX == snake.front().iX && i->iY == snake.front().iY)
					bIsDead = true;
			// // game logic // //

			// Chop off snakes tail :(
			snake.pop_back();

			// Display
				// Clear Screen
			for (int i = 0; i < nScreenWidth * nScreenHeight; i++) screen[i] = L' ';

			// Draw stats and border
			for (int i = 0; i < nScreenWidth; i++)
			{
				screen[i] = L'=';
				screen[2 * nScreenWidth + i] = L'=';
				screen[(nScreenHeight - 1) * nScreenWidth + i] = L'=';
			}
			
			// HUD
				// If player has a personal best, show it, otherwise show --
				// Personal best is only loaded once per game over to minimize database queries
			wsprintf(&screen[nScreenWidth], L"  SCORE: %d", iScore);
			if (iPersonalBest > 0 || iScore > 0)
				wsprintf(&screen[nScreenWidth + 60], L"BEST: %d", iPersonalBest > iScore ? iPersonalBest : iScore);
			else
				wsprintf(&screen[nScreenWidth + 60], L"BEST: --");

			
			// Draw Snake
			for (auto s : snake)
				screen[s.iY * nScreenWidth + s.iX] = bIsDead ? L'+' : L'O';
			// Snake Head
			screen[snake.front().iY * nScreenWidth + snake.front().iX] = bIsDead ? L'X' : L'@';

			// Draw Food
			screen[iFoodY * nScreenWidth + iFoodX] = L'%';

			// If dead, print message, gather name, save score and display high scores
			if (bIsDead)
			{
				if (!bScoreSaved)
				{
					wchar_t wsName[20];
					DrawDeathScreen(screen, nScreenWidth, wsPlayerName, iScore);
					WriteConsoleOutputCharacter(hConsole, screen, nScreenWidth * nScreenHeight, { 0,0 }, &dwBytesWritten);

					if (!bNameEntered)
					{
						GetPlayerName(wsName, 20, screen, nScreenWidth, hConsole);
						wcscpy_s(wsPlayerName, 20, wsName);
						bNameEntered = true;
					}

					SaveScore(wsPlayerName, iScore);
					iPersonalBest = LoadPersonalBest(wsPlayerName);
					bScoreSaved = true;
					wsprintf(&screen[14 * nScreenWidth + 40], L"                                        ");
				}

				DisplayScores(screen, nScreenWidth, nScreenHeight, iScore); //obviously this can be optimized to not query the database every frame, but it's only a snake game so who cares
				
				// Game over message
				DrawDeathScreen(screen, nScreenWidth, wsPlayerName, iScore);
			}

			// Display Frame
			WriteConsoleOutputCharacter(hConsole, screen, nScreenWidth * nScreenHeight, { 0,0 }, &dwBytesWritten);
		}

		// Wait for space key
		while ((0x8000 & GetAsyncKeyState((unsigned char)('\x20'))) == 0);
	}

	return 0;
}