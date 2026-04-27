#include <raylib.h>
#include <thread>
#include <chrono>

#define SQUARE_SIZE             40
#define GRID_HORIZONTAL_SIZE    12
#define GRID_VERTICAL_SIZE      20
#define LATERAL_SPEED           10
#define TURNING_SPEED           12
#define FAST_FALL_AWAIT_COUNTER 30
#define FADING_TIME             33

 int screenWidth = 940;
 int screenHeight = 900;

// Game constants for level progression and gravity speed
class GameConstants {
public:
	static const int MAX_LEVEL = 8;
	static const int LINES_FOR_LEVEL_UP = 10;
	static const int INITIAL_GRAVITY_SPEED = 60;
};

// Check if the mouse is inside a given circle (used for button detection)
bool IsMouseInsideCircle(int centerX, int centerY, int radius) {
	Vector2 mousePosition = GetMousePosition();
	double distance = sqrt(pow(mousePosition.x - centerX, 2) + pow(mousePosition.y - centerY, 2));
	return distance <= radius;
}

// Base Tetromino class, represents a 4x4 grid piece
class Tetromino {
public:
	Color color;
	int grid[4][4] = { 0 };
	Tetromino(Color c) : color(c) {}
	virtual ~Tetromino() {}

	Color GetColor() const
	{ return color; }
	int GetGrid(int i, int j) const
	{ return grid[i][j]; }

	void CopyToGrid(int target[4][4]) const {
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				target[i][j] = grid[i][j];
			}
		}
	}

	// Default rotation: 90 degrees clockwise
	virtual void Rotate(int rotated[4][4]) const {
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				rotated[i][j] = grid[3 - j][i];
			}
		}
	}
};

// Tetromino shape definitions
class TetrominoI : public Tetromino {
public:
	TetrominoI() : Tetromino({ 0, 160, 180, 255 }) {
		grid[0][1] = grid[1][1] = grid[2][1] = grid[3][1] = 1;
	}
};

class TetrominoO : public Tetromino {
public:
	TetrominoO() : Tetromino({ 220, 200, 0, 255 }) {
		grid[1][1] = grid[2][1] = grid[1][2] = grid[2][2] = 1;
	}

};

class TetrominoL : public Tetromino {
public:
	TetrominoL() : Tetromino({ 200, 120, 0, 255 }) {
		grid[1][0] = grid[1][1] = grid[1][2] = grid[2][2] = 1;
	}
};

class TetrominoJ : public Tetromino {
public:
	TetrominoJ() : Tetromino({ 0, 0, 180, 255 }) {
		grid[1][2] = grid[2][0] = grid[2][1] = grid[2][2] = 1;
	}
};

class TetrominoT : public Tetromino {
public:
	TetrominoT() : Tetromino({ 100, 0, 100, 255 }) {
		grid[1][0] = grid[1][1] = grid[1][2] = grid[2][1] = 1;
	}
};

class TetrominoS : public Tetromino {
public:
	TetrominoS() : Tetromino({ 0, 160, 0, 255 }) {
		grid[1][1] = grid[2][1] = grid[2][2] = grid[3][2] = 1;
	}
};

class TetrominoZ : public Tetromino {
public:
	TetrominoZ() : Tetromino({ 180, 0, 0, 255 }) {
		grid[1][2] = grid[2][2] = grid[2][1] = grid[3][1] = 1;
	}
};

// Factory for creating random Tetromino pieces
class TetrominoFactory {
public:
	static Tetromino* CreateRandomTetromino() {
		int random = GetRandomValue(0, 7);

		switch (random) {
		case 0: return new TetrominoO();
		case 1: return new TetrominoL();
		case 2: return new TetrominoJ();
		case 3: return new TetrominoI();
		case 4: return new TetrominoT();
		case 5: return new TetrominoS();
		case 6: return new TetrominoZ();
		case 7: return new TetrominoI();
		}
	}
};

// Represents the Tetris game board and its state
class GameBoard {
private:
	int grid[GRID_HORIZONTAL_SIZE][GRID_VERTICAL_SIZE];
	Color gridColors[GRID_HORIZONTAL_SIZE][GRID_VERTICAL_SIZE];
	Color fadingColor;
	int fadeLineCounter;

public:
	GameBoard() : fadeLineCounter(0), fadingColor(MAROON) {
		InitializeGrid();
	}

	// Initialize the board with borders and empty cells
	void InitializeGrid() {
		for (int i = 0; i < GRID_HORIZONTAL_SIZE; i++) {
			for (int j = 0; j < GRID_VERTICAL_SIZE; j++) {
				if ((j == GRID_VERTICAL_SIZE - 1) || (i == 0) || (i == GRID_HORIZONTAL_SIZE - 1))
					grid[i][j] = 3; // Border cell
				else
					grid[i][j] = 0; // Empty cell

				gridColors[i][j] = BLACK;
			}
		}
	}

	int GetCell(int x, int y) const { return grid[x][y]; }
	void SetCell(int x, int y, int value) { grid[x][y] = value; }
	Color GetCellColor(int x, int y) const { return gridColors[x][y]; }
	void SetCellColor(int x, int y, Color c) { gridColors[x][y] = c; }

	int GetFadeLineCounter() const { return fadeLineCounter; }
	void IncrementFadeLineCounter() { fadeLineCounter++; }
	void ResetFadeLineCounter() { fadeLineCounter = 0; }

	// Check for completed lines and mark them for deletion
	void CheckCompletion(bool* lineToDelete) {
		for (int j = GRID_VERTICAL_SIZE - 2; j >= 0; j--) {
			int count = 0;
			for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++) {
				if (grid[i][j] == 2)
					count++;
			}

			if (count == GRID_HORIZONTAL_SIZE - 2) {
				*lineToDelete = true;
				for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++) {
					grid[i][j] = 4; // Mark for fading
				}
			}
		}
	}

	// Delete lines marked for removal and shift above lines down
	int DeleteCompleteLines() {
		int deletedLines = 0;
		bool once2 = true;

		for (int j = GRID_VERTICAL_SIZE - 2; j >= 0; ) {
			if (grid[1][j] == 4) {
				for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++) {
					if (once2) {
						Sound deleteSound = LoadSound("delete.wav");
						PlaySound(deleteSound);
						once2 = false;
					}
					grid[i][j] = 0;
				}

				// Shift all rows above down by one
				for (int k = j - 1; k >= 0; k--) {
					for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++) {
						grid[i][k + 1] = grid[i][k];
						gridColors[i][k + 1] = gridColors[i][k];
					}
				}

				deletedLines++;
			}
			else {
				j--;
			}
		}

		return deletedLines;
	}

	// Draw the board and its contents
	void Draw(int offsetX, int offsetY) const {
		for (int j = 0; j < GRID_VERTICAL_SIZE; j++) {
			for (int i = 0; i < GRID_HORIZONTAL_SIZE; i++) {
				if (grid[i][j] == 0) {
					DrawRectangleLines(offsetX + i * SQUARE_SIZE, offsetY + j * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE, BLACK);
				}
				else if (grid[i][j] == 2) {
					DrawRectangle(offsetX + i * SQUARE_SIZE, offsetY + j * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE, gridColors[i][j]);
				}
				else if (grid[i][j] == 1) {
					DrawRectangle(offsetX + i * SQUARE_SIZE, offsetY + j * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE, gridColors[i][j]);
				}
				else if (grid[i][j] == 4) {
					DrawRectangle(offsetX + i * SQUARE_SIZE, offsetY + j * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE, fadingColor);
				}
			}
		}
	}

	// Game over if any fixed block is in the top two rows
	bool CheckGameOver() const {
		for (int j = 0; j < 2; j++) {
			for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++) {
				if (grid[i][j] == 2) {
					return true;
				}
			}
		}
		return false;
	}
};

// Represents the currently falling piece
class ActivePiece {
private:
	int piece[4][4];
	int posX, posY;
	Color color;

public:
	ActivePiece() : posX(0), posY(0), color(BLACK) {
		ClearGrid();
	}

	void ClearGrid() {
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				piece[i][j] = 0;
			}
		}
	}

	void SetPosition(int x, int y) {
		posX = x;
		posY = y;
	}

	int GetX() const { return posX; }
	int GetY() const { return posY; }
	void MoveDown() { posY++; }
	void MoveLeft() { posX--; }
	void MoveRight() { posX++; }

	Color GetColor() const { return color; }
	void SetColor(Color c) { color = c; }

	void SetPiece(const Tetromino* tetromino) {
		tetromino->CopyToGrid(piece);
		color = tetromino->GetColor();
	}

	int GetGrid(int x, int y) const {
		if (x >= 0 && x < 4 && y >= 0 && y < 4) {
			return piece[x][y];
		}
		return 0;
	}

	// Rotate the piece 90 degrees clockwise
	void Rotate() {
		int rotated[4][4] = { 0 };

		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				rotated[i][j] = piece[3 - j][i];
			}
		}

		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				piece[i][j] = rotated[i][j];
			}
		}
	}

	// Place the piece on the board at its current position
	void PlaceOnBoard(GameBoard& board) {
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				if (piece[i][j] == 1) {
					board.SetCell(posX + i, posY + j, 1);
					board.SetCellColor(posX + i, posY + j, color);
				}
			}
		}
	}

	// Remove the piece from the board (before moving or rotating)
	void RemoveFromBoard(GameBoard& board) {
		for (int j = 0; j < GRID_VERTICAL_SIZE; j++) {
			for (int i = 0; i < GRID_HORIZONTAL_SIZE; i++) {
				if (board.GetCell(i, j) == 1) {
					board.SetCell(i, j, 0);
				}
			}
		}
	}

	// Check if moving or rotating the piece would cause a collision
	bool CheckCollision(const GameBoard& board, int offsetX, int offsetY) const {
		for (int j = 0; j < 4; j++) {
			for (int i = 0; i < 4; i++) {
				if (piece[i][j] == 1) {
					int newX = posX + i + offsetX;
					int newY = posY + j + offsetY;

					if (newX < 0 || newX >= GRID_HORIZONTAL_SIZE || newY >= GRID_VERTICAL_SIZE) {
						return true;
					}

					if (board.GetCell(newX, newY) == 2 || board.GetCell(newX, newY) == 3) {
						return true;
					}
				}
			}
		}
		return false;
	}
};

// Handles all rendering and texture management
class Renderer {
private:
	Texture2D background, background2, background3, background4a, background4b, background4c, background5;
	Color front;

public:
	Renderer() : front({ 45, 64, 80, 255 }) {
		LoadTextures();
	}

	~Renderer() {
		UnloadTextures();
	}

	void LoadTextures() {
		background = LoadTexture("back.jpg");
		background2 = LoadTexture("back2.jpg");
		background3 = LoadTexture("back3.jpg");
		background5 = LoadTexture("back5.jpg");
	}

	void UnloadTextures() {
		UnloadTexture(background);
		UnloadTexture(background2);
		UnloadTexture(background3);
		UnloadTexture(background5);
	}

	// Show a countdown before the game starts
	void DisplayCountdown() {
		background4a = LoadTexture("back4a.jpg");
		background4b = LoadTexture("back4b.jpg");
		background4c = LoadTexture("back4c.jpg");

		for (int i = 3; i > 0; --i) {
			BeginDrawing();
			DrawTexture(background, 0, 0, WHITE);
			if (i == 3)
				DrawTexture(background4a, 0, 0, WHITE);
			else if (i == 2)
				DrawTexture(background4b, 0, 0, WHITE);
			else if (i == 1)
				DrawTexture(background4c, 0, 0, WHITE);

			EndDrawing();
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		UnloadTexture(background4a);
		UnloadTexture(background4b);
		UnloadTexture(background4c);
	}

	// Draw the game state, including board, next piece, and UI
	void DrawGame(bool pause, bool gameOver, const GameBoard& board,
		int incomingPiece[4][4], Color nextPieceColor,
		int lines, int level) {
		BeginDrawing();

		if (!pause)
			DrawTexture(background, 0, 0, WHITE);
		else
			DrawTexture(background2, 0, 0, WHITE);

		if (!gameOver) {
			int offsetX = 75;
			int offsetY = 55;

			board.Draw(offsetX, offsetY);

			int nextPieceOffsetX = 650;
			int nextPieceOffsetY = 492;

			// Draw the next piece preview
			for (int j = 0; j < 4; j++) {
				for (int i = 0; i < 4; i++) {
					if (incomingPiece[i][j] == 1) {
						DrawRectangle(nextPieceOffsetX + i * SQUARE_SIZE, nextPieceOffsetY + j * SQUARE_SIZE,
							SQUARE_SIZE, SQUARE_SIZE, nextPieceColor);
					}
					else {
						DrawRectangleLines(nextPieceOffsetX + i * SQUARE_SIZE, nextPieceOffsetY + j * SQUARE_SIZE,
							SQUARE_SIZE, SQUARE_SIZE, BLACK);
					}
				}
			}

			DrawText(TextFormat("%01d", lines), 730, 735, 40, front);
			DrawText(TextFormat("%01i", lines * 100), 730, 775, 40, front);
			DrawText(TextFormat("%01i", level), 730, 815, 40, front);
		}
		else {
			if (level > GameConstants::MAX_LEVEL) {
				DrawTexture(background5, 0, 0, WHITE);
			}
			else {
				DrawTexture(background3, 0, 0, WHITE);
				DrawText(TextFormat("%01i", lines * 100), 730, 775, 40, front);
			}
		}

		EndDrawing();
	}
};

// Main game controller: handles game logic, state, and input
class GameController {
private:
	GameBoard board;
	ActivePiece activePiece;
	Renderer renderer;

	int incomingPiece[4][4];
	Tetromino* nextTetromino;

	bool gameOver;
	bool pause;
	bool once;
	bool beginPlay;
	bool pieceActive;
	bool detection;
	bool lineToDelete;

	int level;
	int lines;
	int gravitySpeed;

	int gravityMovementCounter;
	int lateralMovementCounter;
	int turnMovementCounter;
	int fastFallMovementCounter;

public:
	GameController() :
		gameOver(false),
		pause(false),
		once(false),
		beginPlay(true),
		pieceActive(false),
		detection(false),
		lineToDelete(false),
		level(1),
		lines(0),
		gravitySpeed(GameConstants::INITIAL_GRAVITY_SPEED),
		gravityMovementCounter(0),
		lateralMovementCounter(0),
		turnMovementCounter(0),
		fastFallMovementCounter(0),
		nextTetromino(nullptr) {

		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				incomingPiece[i][j] = 0;
			}
		}
	}

	~GameController() {
		if (nextTetromino) {
			delete nextTetromino;
		}
	}

	// Reset all game state to start a new game
	void InitGame() {
		level = 1;
		lines = 0;
		activePiece.SetPosition(0, 0);
		pause = false;
		beginPlay = true;
		pieceActive = false;
		detection = false;
		lineToDelete = false;

		gravitySpeed = GameConstants::INITIAL_GRAVITY_SPEED;
		gravityMovementCounter = 0;
		lateralMovementCounter = 0;
		turnMovementCounter = 0;
		fastFallMovementCounter = 0;
		board.ResetFadeLineCounter();

		board.InitializeGrid();

		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				incomingPiece[i][j] = 0;
			}
		}

		if (nextTetromino) {
			delete nextTetromino;
		}

		nextTetromino = TetrominoFactory::CreateRandomTetromino();

		renderer.DisplayCountdown();
		Sound startSound = LoadSound("start.wav");
		PlaySound(startSound);
	}

	// Generate a new random piece for the next turn
	void GetRandomPiece() {
		if (nextTetromino) {
			delete nextTetromino;
		}

		nextTetromino = TetrominoFactory::CreateRandomTetromino();

		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				incomingPiece[i][j] = 0;
			}
		}

		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				incomingPiece[i][j] = nextTetromino->GetGrid(i, j);
			}
		}
	}

	// Place a new active piece on the board
	bool CreatePiece() {
		int piecePositionX = (GRID_HORIZONTAL_SIZE - 4) / 2;
		int piecePositionY = 0;

		activePiece.SetPosition(piecePositionX, piecePositionY);

		if (beginPlay) {
			GetRandomPiece();
			beginPlay = false;
		}

		activePiece.SetPiece(nextTetromino);

		GetRandomPiece();

		activePiece.PlaceOnBoard(board);

		return true;
	}

	// Handle the downward movement of the active piece
	void ResolveFallingMovement() {
		if (detection) {
			// Piece has landed, convert to fixed blocks
			for (int j = GRID_VERTICAL_SIZE - 2; j >= 0; j--) {
				for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++) {
					if (board.GetCell(i, j) == 1) {
						board.SetCell(i, j, 2);
						detection = false;
						pieceActive = false;
					}
				}
			}
		}
		else {
			// Move the piece down by one row
			for (int j = GRID_VERTICAL_SIZE - 2; j >= 0; j--) {
				for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++) {
					if (board.GetCell(i, j) == 1) {
						board.SetCell(i, j + 1, 1);
						board.SetCellColor(i, j + 1, board.GetCellColor(i, j));
						board.SetCell(i, j, 0);
					}
				}
			}
			activePiece.MoveDown();
		}
	}

	// Handle left/right movement input for the active piece
	bool ResolveLateralMovement() {
		bool collision = false;

		if (IsKeyDown(KEY_LEFT)) {
			if (activePiece.CheckCollision(board, -1, 0)) {
				collision = true;
			}

			if (!collision) {
				activePiece.RemoveFromBoard(board);

				activePiece.MoveLeft();

				activePiece.PlaceOnBoard(board);
			}
		}
		else if (IsKeyDown(KEY_RIGHT)) {
			if (activePiece.CheckCollision(board, 1, 0)) {
				collision = true;
			}

			if (!collision) {
				activePiece.RemoveFromBoard(board);

				activePiece.MoveRight();

				activePiece.PlaceOnBoard(board);
			}
		}

		return collision;
	}

	// Handle rotation input for the active piece
	bool ResolveTurnMovement() {
		if (IsKeyDown(KEY_UP)) {
			activePiece.RemoveFromBoard(board);

			activePiece.Rotate();

			activePiece.PlaceOnBoard(board);

			return true;
		}

		return false;
	}

	// Check if the active piece should stop falling (collision below)
	void CheckDetection() {
		for (int j = GRID_VERTICAL_SIZE - 2; j >= 0; j--) {
			for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++) {
				if ((board.GetCell(i, j) == 1) &&
					((board.GetCell(i, j + 1) == 2) || (board.GetCell(i, j + 1) == 3))) {
					detection = true;
				}
			}
		}
	}

	// Main game update loop: handles input, movement, line clearing, and game state
	void UpdateGame() {
		if (!gameOver) {
			if (level > GameConstants::MAX_LEVEL) {
				gameOver = true;
				return;
			}

			// Pause button (mouse click on pause circle)
			if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && IsMouseInsideCircle(807, 98, 45)) {
				Sound pauseSound = LoadSound("pause.wav");
				PlaySound(pauseSound);
				pause = !pause;
			}

			if (!pause) {
				// Level up if enough lines cleared
				if (lines >= level * GameConstants::LINES_FOR_LEVEL_UP) {
					level++;
					gravitySpeed = (int)(GameConstants::INITIAL_GRAVITY_SPEED * pow(0.9, level - 1));
					if (level > GameConstants::MAX_LEVEL) {
						gameOver = true;
						return;
					}
				}

				if (!lineToDelete) {
					if (!pieceActive) {
						pieceActive = CreatePiece();
						fastFallMovementCounter = 0;
					}
					else {
						fastFallMovementCounter++;
						gravityMovementCounter++;
						lateralMovementCounter++;
						turnMovementCounter++;

						// Reset movement counters on key press for responsiveness
						if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT))
							lateralMovementCounter = LATERAL_SPEED;

						if (IsKeyPressed(KEY_UP))
							turnMovementCounter = TURNING_SPEED;

						// Fast fall if down key held
						if (IsKeyDown(KEY_DOWN) && (fastFallMovementCounter >= FAST_FALL_AWAIT_COUNTER)) {
							gravityMovementCounter += gravitySpeed;
						}

						// Handle gravity (piece falling)
						if (gravityMovementCounter >= gravitySpeed) {
							CheckDetection();
							ResolveFallingMovement();
							board.CheckCompletion(&lineToDelete);
							gravityMovementCounter = 0;
						}

						// Handle lateral movement
						if (lateralMovementCounter >= LATERAL_SPEED) {
							if (!ResolveLateralMovement())
								lateralMovementCounter = 0;
						}

						// Handle rotation
						if (turnMovementCounter >= TURNING_SPEED) {
							if (ResolveTurnMovement())
								turnMovementCounter = 0;
						}
					}

					// Check for game over condition
					if (board.CheckGameOver()) {
						once = true;
						gameOver = true;
					}
				}
				else {
					// Handle line fading animation before deletion
					board.IncrementFadeLineCounter();

					if (board.GetFadeLineCounter() >= FADING_TIME) {
						int deletedLines = board.DeleteCompleteLines();
						board.ResetFadeLineCounter();
						lineToDelete = false;
						lines += deletedLines;
					}
				}
			}
		}
		else {
			// Restart game on mouse click after game over
			if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && IsMouseInsideCircle(807, 98, 45)) {
				gameOver = false;
				InitGame();
			}
		}
	}

	// Draw the current game state
	void DrawGame() {
		renderer.DrawGame(pause, gameOver, board, incomingPiece, nextTetromino->GetColor(), lines, level);

		// Play game over sound only once
		if (once) {
			Sound gameOverSound = LoadSound("over.wav");
			PlaySound(gameOverSound);
			once = false;
		}
	}

	// Main frame update: update logic and render
	void UpdateDrawFrame() {
		UpdateGame();
		DrawGame();
	}
};

int main(void) {
	InitWindow(screenWidth, screenHeight, "TETRIS");
	InitAudioDevice();

	GameController game;
	game.InitGame();

	SetTargetFPS(60);

	while (!WindowShouldClose()) {
		game.UpdateDrawFrame();
	}

	CloseWindow();

	return 0;
}
