#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <ctime>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
using namespace std;

const int sharedMemSize = 1024; // Size of shared memory segment
const char* sharedMemName = "potion_explosion";
const int semKey = 1234; // Key for semaphore
void* sharedMem; // Global variable for shared memory

const int totalBalls = 80;
const int totalColors = 4;

string dispenser[totalBalls / totalColors][totalColors];
string ballColors[totalColors] = { "Red", "Yellow", "Blue", "Black" };

struct Potion {
	string name;
	vector<string> ingredients;
	int score;
};

struct Player {
	string name;
	int score;
	vector<string> dispenserRow;
	vector<Potion> potions;
	vector<Potion> completedPotions;
};

void removeExistingSharedMemory() {
	shm_unlink(sharedMemName);
}

void createSharedMemory() {
	int shm_fd = shm_open(sharedMemName, O_RDWR | O_CREAT | O_EXCL, 0666);
	if (shm_fd == -1) {
		perror("shm_open");
		exit(EXIT_FAILURE);
	}

	ftruncate(shm_fd, sharedMemSize);

	sharedMem = mmap(NULL, sharedMemSize, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (sharedMem == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	close(shm_fd);
}

void updateSharedMem(const string& message) {
	// Update shared memory with the message
	strcpy((char*)sharedMem, message.c_str());
}

string readSharedMem() {
	// Read message from shared memory
	return string((char*)sharedMem);
}

// Semaphore functions
int createSemaphore() {
	key_t key = ftok(".", semKey);
	return semget(key, 1, IPC_CREAT | 0666);
}

void initSemaphore(int semid, int value) {
	semctl(semid, 0, SETVAL, value); // Set semaphore value
}

void waitSemaphore(int semid) {
	struct sembuf sb = { 0, -1, 0 }; // Down operation
	semop(semid, &sb, 1);
}

void signalSemaphore(int semid) {
	struct sembuf sb = { 0, 1, 0 }; // Up operation
	semop(semid, &sb, 1);
}

// Function to initialize a player
Player initializePlayer(int playerNumber) {
	Player player;
	cout << "Enter Player " << playerNumber << "'s name: ";
	getline(cin, player.name);
	player.score = 0;
	player.dispenserRow.resize(totalColors);
	player.potions.resize(0);
	return player;
}

// Function to initialize the dispenser with random balls
void initializeDispenser() {
	int redBalls = 0, yellowBalls = 0, blueBalls = 0, blackBalls = 0;

	// Seed the random number generator with the current time
	std::srand(static_cast<unsigned>(std::time(0)));

	for (int i = 0; i < totalBalls / totalColors; i++) {
		for (int j = 0; j < totalColors; j++) {
			int randomNumber = std::rand() % totalColors;
			dispenser[i][j] = ballColors[randomNumber];

			// Adjust color count
			switch (randomNumber) {
			case 0: redBalls++; break;
			case 1: yellowBalls++; break;
			case 2: blueBalls++; break;
			case 3: blackBalls++; break;
			}

			// Check if a color exceeds the limit
			if (redBalls > totalBalls / totalColors || yellowBalls > totalBalls / totalColors || blueBalls > totalBalls / totalColors || blackBalls > totalBalls / totalColors) {
				if (redBalls < totalBalls / totalColors) {
					dispenser[i][j] = ballColors[0];
				}
				else if (yellowBalls < totalBalls / totalColors) {
					dispenser[i][j] = ballColors[1];
				}
				else if (blueBalls < totalBalls / totalColors) {
					dispenser[i][j] = ballColors[2];
				}
				else {
					dispenser[i][j] = ballColors[3];
				}
			}
		}
	}
}

bool isPotionComplete(Player& player, const Potion& potion) {
	for (const string& ingredient : potion.ingredients) {
		if (count(player.dispenserRow.begin(), player.dispenserRow.end(), ingredient) < 1) {
			return false;
		}
	}
	return true;
}

void brewPotion(Player& player, const Potion& potion) {
	for (const string& ingredient : potion.ingredients) {
		player.dispenserRow.erase(std::find(player.dispenserRow.begin(), player.dispenserRow.end(), ingredient));
	}
	player.potions.push_back(potion);
	player.score += potion.score;
	cout << player.name << " successfully brewed a " << potion.name << " potion!\n";
	cout << player.name << "'s Score: " << player.score << "\n";
}

void displayDispenser(Player& player) {
	cout << "\n\t\t\tDispenser\n";
	cout << "\t\t\t----------\n";

	// Display column headers
	cout << "      ";
	for (int i = 0; i < totalColors; i++) {
		cout << setw(11) << "Column " << (i + 1);
	}
	cout << "\n";

	// Display dispenser content
	for (int i = 0; i < totalBalls / totalColors; i++) {
		cout << "\t" << setw(2) << i + 1 << ". ";
		for (int j = 0; j < totalColors; j++) {
			cout << setw(10) << dispenser[i][j];
		}
		cout << "\n";
	}

	// Display player's dispenser row
	cout << "\t" << player.name << "'s Dispenser Row: ";
	for (const string& ball : player.dispenserRow) {
		cout << ball << " ";
	}
	cout << "\n";
}

// Function to display the current selected potion tiles and completed potion tiles for a player
void displayPotionTiles(const Player& player) {
	// Display current selected potion tiles
	cout << "Current Potion Tiles:\n";
	int count = 1;
	for (const Potion& potion : player.potions) {
		cout << count++ << ". " << potion.name << " (Score: " << potion.score << ")\n";
		cout << "\t=> { ";
		for (const string& ingredient : potion.ingredients) {
			cout << ingredient << ", ";
		}
		cout << " }\n";
	}
	cout << "\n\n";

	// Display completed potion tiles
	cout << "Completed Potion Tiles : \n";
	count = 1;
	for (const Potion& completedPotion : player.completedPotions) {
		cout << count++ << ". " << completedPotion.name << " (Score: " << completedPotion.score << ")\n";
		cout << "\t=> { ";
		for (const string& ingredient : completedPotion.ingredients) {
			cout << ingredient << ", ";
		}
		cout << " }\n";
	}
}

void displayAvailableBalls(const Player& player) {
	cout << "Available Balls: ";
	for (const string& ball : player.dispenserRow) {
		cout << ball << ", ";
	}
	cout << endl;
}

void addAvailableBalls(Player& player, int row, int column) {
	// Remove the selected ball from the dispenser
	player.dispenserRow.push_back(dispenser[row - 1][column - 1]);
	dispenser[row - 1][column - 1] = "-";

	int up = row - 2, down = row;
	while (up >= 0 && down <= totalBalls / totalColors) {
		if (dispenser[up][column - 1] == dispenser[down][column - 1]) {

			player.dispenserRow.push_back(dispenser[down][column - 1]);
			dispenser[down][column - 1] = "-";
			down++;
			while (dispenser[up][column - 1] == dispenser[down][column - 1] && down <= totalBalls / totalColors) {
				player.dispenserRow.push_back(dispenser[down][column - 1]);
				dispenser[down][column - 1] = "-";
				down++;
			}
			
			player.dispenserRow.push_back(dispenser[up][column - 1]);
			string ballColor = dispenser[up][column - 1];
			dispenser[up][column - 1] = "-";
			up--;
			while (ballColor == dispenser[up][column - 1] && up >= 0) {
				player.dispenserRow.push_back(dispenser[up][column - 1]);
				dispenser[up][column - 1] = "-";
				up--;;
			}
		}
	}

	for (int i = row - 1; i > 0; i--) {
		swap(dispenser[i][column - 1], dispenser[i - 1][column - 1]);
	}
}

void displayWelcomeMessage() {
	system("clear"); // Clear the console screen
	cout << "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\t\t\t\t\t\t\t\t\t\t\tWelcome to Potion Explosion!\n\n\n\n\n";
	cout << "Press Enter to start...\n\n";
	cin.ignore(); // Wait for user input (Enter)
}

void choosePotionTiles(Player& player, vector<Potion> potions) {
	system("clear");
	int i = 1;
	cout << "Available Potion Tiles:\n";
	for (const Potion& potion : potions) {
		cout << i << ". " << potion.name << " (Score: " << potion.score << ")\n { ";
		for (const string& ingredient : potion.ingredients) {
			cout << ingredient << ", ";
		}
		cout << " }\n";
	}

	if (player.potions.size() == 0) {
		int potion1, potion2;
		cout << "Choose tiles by entering their numbers (e.g., 1 3 OR 1 4): ";
		cin >> potion1 >> potion2;
		player.potions.push_back(potions[potion1 - 1]);
		player.potions.push_back(potions[potion2 - 1]);
	}
	else {
		int potion1;
		cout << "Choose a tile by entering their number: ";
		cin >> potion1;
		player.potions.push_back(potions[potion1 - 1]);
	}
}

int main() {
	displayWelcomeMessage();
	system("clear");
	initializeDispenser();

	// Define available potions
	vector<Potion> potions;
	potions.push_back(Potion{ "Fireball", {"Red", "Yellow"}, 10 });
	potions.push_back(Potion{ "Ice Blast", {"Blue", "Black"}, 15 });
	potions.push_back(Potion{ "Abyssal Draft", {"Red", "Yellow", "Blue", "Black"}, 8 });
	potions.push_back(Potion{ "Magnetic Attraction", {"Black", "Black"}, 6 });
	potions.push_back(Potion{ "Prismatic Joy", {"Yellow", "Blue", "Red", "Red"}, 3 });

	srand(static_cast<unsigned>(time(nullptr)));

	removeExistingSharedMemory();
	createSharedMemory();

	// Create a semaphore to synchronize turns
	int semid = createSemaphore();
	initSemaphore(semid, 1); // Initialize semaphore value to 1

	// Initialize players
	Player player1 = initializePlayer(1);
	Player player2 = initializePlayer(2);

	// Fork a new process for player 2
	pid_t pid = fork();

	if (pid < 0) {
		cerr << "Fork failed!\n";
		exit(EXIT_FAILURE);
	}
	else if (pid == 0) {
		// Child process (player 2)
		while (true) {
			waitSemaphore(semid); // Wait for turn

			if (player2.potions.size() < 2) {
				choosePotionTiles(player2, potions);
			}

			displayDispenser(player2);
			displayPotionTiles(player2);

			// Player 2 selects a ball from the dispenser
			cout << player2.name << ", it's your turn. Select a ball from the dispenser (enter row and column e.g: 1 2): ";
			int selectedRow, selectedColumn;
			do {
				cin >> selectedRow >> selectedColumn;
				if ((selectedRow > totalBalls / totalColors || selectedRow < 1) && (selectedColumn > totalColors || selectedColumn < 1)) {
					cout << "Invalid Input. Enter again.";
				}
			} while ((selectedRow > totalBalls / totalColors || selectedRow < 1) && (selectedColumn > totalColors || selectedColumn < 1));

			// Update the dispenser after the player picks a ball and also checking for explosions
			addAvailableBalls(player2, selectedRow, selectedColumn);

			system("clear");
			displayDispenser(player2);
			displayPotionTiles(player2);
			displayAvailableBalls(player2);

			// Ask user to place the balls on Potion Tiles
			while (true) {
				int tile;
				do {
					cout << "Press the Potion tile number to select it: ";
					cin >> tile;
					if (tile < 1 || tile > 2) {
						cout << "Invalid input.Enter Again!\n";
					}
				} while (tile < 1 || tile > 2);

				while (true) {
					cout << "Press to select the ball:\n";
					int selectBall;
					cout << "0. Red\n1. Yellow\n2. Blue\n3. Black\n-1. Return to previous step.\n";
					do {
						cin >> selectBall;
						if (selectBall < -1 || selectBall>3) {
							cout << "Invalid input. Enter Again!\n";
						}
					} while (selectBall < -1 || selectBall>3);
					if (selectBall == -1) {
						break;
					}
					player2.potions[tile - 1].ingredients.push_back(ballColors[selectBall]);
					player2.dispenserRow.erase(std::find(player2.dispenserRow.begin(), player2.dispenserRow.end(), ballColors[selectBall]));
				}
				
			}

			
			// Update shared memory with player's turn information
			updateSharedMem(player1.name + " completed their turn.");

			signalSemaphore(semid); // Signal turn completion
			sleep(1); // Add a delay for better readability
			system("clear");
		}

	}
	else {
		// Parent process (player 1)
		while (true) {
			waitSemaphore(semid); // Wait for turn

			displayDispenser(player1);
			displayPotionTiles(player1);
			displayAvailableBalls(player1);

			// Check for available potions and brew if possible
			for (const Potion& potion : potions) {
				if (isPotionComplete(player1, potion)) {
					brewPotion(player1, potion);
				}
			}

			// Simulate other actions in the turn
			// ...
			// Update shared memory with player's turn information
			updateSharedMem(player2.name + " completed their turn.");

			signalSemaphore(semid); // Signal turn completion
			sleep(1); // Add a delay for better readability
		}
	}

	// Wait for the child process to complete (player 2)
	waitpid(pid, nullptr, 0);

	// Remove the semaphore
	semctl(semid, 0, IPC_RMID, nullptr);

	// Check for game ending conditions (e.g., all balls collected)
	// ...

	// Display final score and winner
	if (player1.score > player2.score) {
		cout << player1.name << " wins! \n";
	}
	else if (player1.score < player2.score) {
		cout << player2.name << " wins! \n";
	}
	else {
		cout << "It's a tie! \n";
	}

	return 0;
}

