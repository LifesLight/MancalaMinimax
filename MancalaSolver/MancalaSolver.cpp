/*
   Mancala Minimax implementation
   Copyright (c) 2022 Alexander Kurtz. All rights reserved.
   This code is distributed under the terms of the MIT License and WITHOUT ANY WARRANTY
*/

#include <iostream>
#include <string>
#include <thread>
#include <ctime>
#include <ctype.h>
/* If compiled on Windows, enable colored console output */
#ifdef _WIN32
    #define NOMINMAX
    #include <Windows.h>
#endif

#define POSITION_LENGTH 14
#define PLAYER_SCORE 6
#define COMPUTER_SCORE 13

/* Check wether all of "Players" or "Computer" array fields are 0 */
#define PlayerEmpty(position) !(*(int64_t*)position & 0x0000FFFFFFFFFFFF)
#define ComputerEmpty(position) !(*(int64_t*)(position+7) & 0x0000FFFFFFFFFFFF)
/* Evaluation for position */
#define Evaluation(position) position[COMPUTER_SCORE] - position[PLAYER_SCORE]

/* 
Design:
    Player/Computer:
        Roles are allocated statically -> Players fields are always 0 - 6 while Computer is always 7 - 13
        The "Player/Computer" terms are mearly for understanding sake, for example "move" returns which "player's" turn
        it is next via player boolean.
        There is free choice which agent gets which "role", you can just make the minimax agent the "player" for example.

    Tree-Search:
        Minimax implementation
        Evaluation is "Computer" positive

    Multithreading:
        Each minimax root call (calculation for each viable FIRST move) is handed of to a seperate threads
*/

/* 
Move function simulates the "playing" of a field on the "position" board. 
Requires info on whose turn the simulation is supposed to be.
It returns whose turn it is next.
*/
bool move(uint8_t* position, uint8_t selection, bool& player)
{
    /* Store "to be distributed" stone count and clear selected field */
    unsigned char count = position[selection];
    position[selection] = 0;
    /* For the amount of stones to be distributed, go trough board position array and add one stone per field */
    for (count ; count > 0; count--)
    {
        selection += 1;
        selection = selection % POSITION_LENGTH;
        /* Skip enemy "Score" fields */
        if (selection == (player ? COMPUTER_SCORE : PLAYER_SCORE))
            count += 1;
        else    
            position[selection] += 1;
    }
    /*
    Check for capture (taking stones from enemy side).
    Checking whose turn it is next and returning that
    */
    if (player)
    {
        if (selection == PLAYER_SCORE)
            return true;
        if (6 > selection && position[selection] == 1 && position[POSITION_LENGTH - selection - 2] > 0)
        {
            position[selection] = 0;
            position[PLAYER_SCORE] += position[POSITION_LENGTH - selection - 2] + 1;
            position[POSITION_LENGTH - selection - 2] = 0;
        }
        return false;
    }
    else
    {
        if (selection == COMPUTER_SCORE)
            return false;
        if (selection > 6 && position[selection] == 1 && position[POSITION_LENGTH - selection - 2] > 0)
        {
            position[selection] = 0;
            position[COMPUTER_SCORE] += position[POSITION_LENGTH - selection - 2] + 1;
            position[POSITION_LENGTH - selection - 2] = 0;
        }
        return true;
    }
}

/*
Tree-Search
Adapted to work with variable turn orders.
Recursive.
Returns the static evaluation of its children.
Optimizing for root "player" call.
*/
int8_t minimax(uint8_t* position, bool player, uint8_t depth, int8_t alpha, int8_t beta)
{
    /* Branch terminating events */
    /* If terminal return evaluation */
    if (PlayerEmpty(position))
    { 
        for (int i = 7; i < 13; i++)
            position[COMPUTER_SCORE] += position[i];
        return Evaluation(position);
    }
    if (ComputerEmpty(position))
    {
        for (int i = 0; i < 6; i++)
            position[PLAYER_SCORE] += position[i];
        return Evaluation(position);
    }
    if (depth == 0)
    {
        return Evaluation(position);
    }

    /* Extend branch */
    /* Each possible move is a new child */
    int8_t ScoreReference;
    /* Maximize/Minimize evaluation depending on who is being optimized */
    if (player)
    {
        /* Reference score is worst possible for lowest possible score */
        ScoreReference = 127;
        for (int i = 0; i < 6; i++)
        {
            /* Don't create child if the target field is empty, so not a viable move */
            if (position[i] == 0)
                continue;
            /* Create independent duplicate of board "position" for child */
            uint8_t PositionCopy[POSITION_LENGTH];
            memcpy(PositionCopy, position, POSITION_LENGTH);
            /* Recursive call, optimizing for whoever move returned next move too */
            ScoreReference = std::min(ScoreReference, minimax(PositionCopy, move(PositionCopy, i, player), depth - 1, alpha, beta));
            /* Alpha-Beta breakoff condition */
            if (ScoreReference <= alpha)
                break;
            /* Update Beta value */
            beta = std::min(ScoreReference, beta);
        }      
    }
    else
    {
        /* Reference score is worst possible for highest possible score */
        ScoreReference = -128;
        for (int i = 7; i < 13; i++)
        {
            if (position[i] == 0)
                continue;
            uint8_t PositionCopy[POSITION_LENGTH];
            memcpy(PositionCopy, position, POSITION_LENGTH);
            ScoreReference = std::max(ScoreReference, minimax(PositionCopy, move(PositionCopy, i, player), depth -1, alpha, beta));
            
            if (ScoreReference >= beta)
                break;
            alpha = std::max(ScoreReference, alpha);
        }
    }

    /* Return evaluation of children */
    return ScoreReference;
}

/* 
Function for individual threads to call, takes "firstMove" argument which determines which 
first branch the thread should search.
*/
void minimaxThreadCall(int8_t* target,uint8_t firstMove, uint8_t* position, bool player, uint8_t depth)
{
    uint8_t PositionCopy[POSITION_LENGTH];
    memcpy(PositionCopy, position, POSITION_LENGTH * sizeof(uint8_t));
    *target = minimax(PositionCopy, move(PositionCopy, firstMove, player), depth - 1, -128, 127);
}

/* Tree-Search root call, returns best possible move with consideration of "depth" amount next moves */
int8_t minimaxRoot(uint8_t* position, bool player, uint8_t depth)
{
    std::thread* workers[6];
    int8_t* results[6];

    for (int i = 0; i < 6; i++)
    {
        if (position[player ? i : i + 7] == 0)
        {
            results[i] = nullptr;
            continue;
        }      
        int8_t* result = new int8_t;
        std::thread* worker = new std::thread(minimaxThreadCall,result, player ? i : i + 7, position, player, depth);
        workers[i] = worker;
        results[i] = result;
    }

    for (int i = 0; i < 6; i++)
        if (results[i] != nullptr)
            workers[i]->join();

    int8_t score = player ? 127 : -128;
    uint8_t bestIndex;

    for (int i = 0; i < 6; i++)
    {
        if (results[i] == nullptr)
            continue;
        if (player && *results[i] < score)
        {
            score = *results[i];
            bestIndex = i;
        }
        else if (!player && *results[i] > score)
        {
            score = *results[i];
            bestIndex = i + 7;
        }
    }

#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    std::cout << "Evaluation: ";
    SetConsoleTextAttribute(hConsole, (score > 0) ? 4 : 9);
    std::cout << (player ? +(-score) : +score) << std::endl;
    SetConsoleTextAttribute(hConsole, 7);
#else
    std::cout << "Evaluation: " << (player ? +(-score) : +score) << std::endl;
#endif
    return bestIndex;
}

/* Print the board "position" */
void print(unsigned char* position)
{
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    std::cout << "   < 0--1--2--3--4--5 >" << std::endl;
    SetConsoleTextAttribute(hConsole, 4);
    std::cout << std::setw(3) << +position[COMPUTER_SCORE];
    SetConsoleTextAttribute(hConsole, 7);
#else
    std::cout << "   < 0--1--2--3--4--5 >" << std::endl;
    std::cout << std::setw(3) << +position[COMPUTER_SCORE];
#endif
    for (int i = COMPUTER_SCORE - 1; i > PLAYER_SCORE; i--)
        std::cout << std::setw(3) << + position[i];
#ifdef _WIN32
    SetConsoleTextAttribute(hConsole, 9);
    std::cout << std::setw(3) << +position[PLAYER_SCORE] << std::endl << "   ";
    SetConsoleTextAttribute(hConsole, 7);
#else
    std::cout << std::setw(3) << +position[PLAYER_SCORE] << std::endl << "   ";
#endif
    for (int i = 0; i < PLAYER_SCORE; i++)
        std::cout << std::setw(3) << +position[i];
    std::cout << std::endl;
}

/* 
Agent class
Stores agent settings and contains move function for each type
Types:
    Random Agent = "random"
    Human Player Agent = "player"
    Minimax Agent = "computer"
*/
class Agent
{
private:
    std::string type;
    uint8_t depth;
public:
    Agent(std::string type)
        : type(type), depth(12)
    {}

    Agent(std::string type, uint8_t depth)
        : type(type), depth(depth)
    {}

    void Move(uint8_t* board, bool& turn)
    {
        /* Minimax */
        if (type == "computer")
        {
            uint8_t cacheResult = minimaxRoot(board, turn, depth);
            std::cout << "Calculated move: " << (turn ? cacheResult : 12 - cacheResult) << std::endl;
            turn = move(board, cacheResult, turn);
        }
        /* Human player */
        else if (type == "player")
        {
            std::string input;
            uint8_t inputMove;
            std::cout << "Move:";
            std::cin >> input;
            inputMove = stoi(input);
            if (0 <= inputMove && inputMove < 6 && board[turn ? inputMove : 12 - inputMove] > 0)
                turn = move(board, turn ? inputMove : 12 - inputMove, turn);
            else
                std::cout << "Invalid Input!" << std::endl;
        }
        /* Random */
        else
        {
            while (true)
            {
                uint8_t selection = std::rand() % 6;
                if (board[turn ? selection : 12 - selection] > 0)
                {
                    std::cout << "Random move: " << +(turn ? 12 - selection : selection) << std::endl;
                    turn = move(board, turn ? selection : 12 - selection, turn);
                    break;
                }
            }
        }
    }
};

/*
Game loop container
Supports random position, custom start position, start selection
and all combinations of different agents playing against each other
*/
class Environment
{
private:
    bool turn;
    Agent agent1;
    Agent agent2;
    uint8_t* position = new uint8_t[POSITION_LENGTH]
    {
        4,4,4,4,4,4,
        0,
        4,4,4,4,4,4,
        0
    };

public:
    Environment(Agent player1, Agent player2, bool playerStart, uint8_t board[])
        : position(board), turn(playerStart), agent1(player1), agent2(player2)
    {}
    
    Environment(Agent player1, Agent player2, bool playerStart)
        : turn(playerStart), agent1(player1), agent2(player2)
    {}

    Environment(Agent player1, Agent player2)
        : turn(true), agent1(player1), agent2(player2)
    {}

    ~Environment()
    {
        delete[] position;
    }

private:
    /* Randomize position with "StoneCount" amount of stones per side */
    void p_RandomizePosition(uint8_t StoneCount)
    {
        std::srand((unsigned int)std::time(nullptr));

        for (int i = 0; i < POSITION_LENGTH; i++)
            position[i] = 0;

        for (int i = 0; i < StoneCount; i++)
            position[std::rand() % 6] += 1;

        for (int i = 0; i < 6; i++)
            position[PLAYER_SCORE + i + 1] = position[i];
    }

public:
    void RandomizePosition()
    { 
        p_RandomizePosition(4 * 6);
    }
    void RandomizePosition(uint8_t stoneCount)
    {
        p_RandomizePosition(stoneCount);
    }

    /* Start game loop */
    void start()
    {
        /*
        Check for too many stones, warning at > 127, not 255 since the evaluation is int8_t 
        so if not using minimax everything until 255 should work
        */
        int count = 0;
        for (int i = 0; i < POSITION_LENGTH; i++)
            count += position[i];
        if (count > 127)
            std::cout << "[WARNING]: Too many stones on field! -> Risk of variable overflow!" << std::endl;

        print(position);
        while (!PlayerEmpty(position) && !ComputerEmpty(position))
        {
            std::cout << " <----<---<-<>->--->---->" << std::endl;
            if (turn)
            {
                std::cout << "AGENT 1" << std::endl;
                agent1.Move(position, turn);
            } 
            else
            {
                std::cout << "AGENT 2" << std::endl;
                agent2.Move(position, turn);
            }
                
            print(position);
        }

        std::cout << " <----<---<-<>->--->---->" << std::endl;

        if (PlayerEmpty(position))
        {
            for (int i = 7; i < 13; i++)
            {
                position[COMPUTER_SCORE] += position[i];
                position[i] = 0;
            }
        }

        if (ComputerEmpty(position))
        {
            for (int i = 0; i < 6; i++)
            {
                position[PLAYER_SCORE] += position[i];
                position[i] = 0;
            }
        }

        print(position);

        if (position[PLAYER_SCORE] > position[COMPUTER_SCORE])
            std::cout << "AGENT 1 WON" << std::endl;
        else if (position[PLAYER_SCORE] < position[COMPUTER_SCORE])
            std::cout << "AGENT 2 WON" << std::endl;
        else
            std::cout << "DRAW!" << std::endl;
    }
};

int main()
{
    Environment game(Agent("player"), Agent("computer", 16), true);
    //game.RandomizePosition();
    game.start();
}