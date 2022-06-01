/*
   Mancala Minimax implementation
   Copyright (c) 2022 Alexander Kurtz. All rights reserved.
   This library is distributed under the terms of the MIT License and WITHOUT ANY WARRANTY
*/

#include <iostream>
#include <string>
#include <thread>
#include <vector>

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

bool move(uint8_t* position, uint8_t selection, bool& player)
{
    unsigned char count = position[selection];
    position[selection] = 0;

    for (count ; count > 0; count--)
    {
        selection += 1;
        selection = selection % POSITION_LENGTH;
        position[selection] += 1;
    }

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

int8_t minimax(uint8_t* position, bool player, uint8_t depth, int8_t alpha, int8_t beta)
{
    /* Branch terminating events */
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
    if (player)
    {
        int8_t ScoreMin = 127;
        for (int i = 0; i < 6; i++)
        {
            if (position[i] == 0)
                continue;
            uint8_t PositionCopy[POSITION_LENGTH];
            memcpy(PositionCopy, position, POSITION_LENGTH);
            ScoreMin = std::min(ScoreMin, minimax(PositionCopy, move(PositionCopy, i, player), depth - 1, alpha, beta));
            
            if (ScoreMin <= alpha)
                break;
            beta = std::min(ScoreMin, beta);
        }
        return ScoreMin;
    }
    else
    {
        int8_t ScoreMax = -128;
        for (int i = 7; i < 13; i++)
        {
            if (position[i] == 0)
                continue;
            uint8_t PositionCopy[POSITION_LENGTH];
            memcpy(PositionCopy, position, POSITION_LENGTH);
            ScoreMax = std::max(ScoreMax, minimax(PositionCopy, move(PositionCopy, i, player), depth -1, alpha, beta));
            
            if (ScoreMax >= beta)
                break;
            alpha = std::max(ScoreMax, alpha);
            
        }
        return ScoreMax;
    }
}

void minimaxThreadCall(int8_t* target,uint8_t firstMove, uint8_t* position, bool player, uint8_t depth)
{
    uint8_t PositionCopy[POSITION_LENGTH];
    memcpy(PositionCopy, position, POSITION_LENGTH * sizeof(uint8_t));
    *target = minimax(PositionCopy, move(PositionCopy, firstMove, player), depth - 1, -128, 127);
}

int8_t minimaxRoot(uint8_t* position, bool player, uint8_t depth)
{
    std::vector <std::thread*> workers;
    std::vector <int8_t*> results;

    for (int i = 0; i < 6; i++)
    {
        if (position[player ? i : i + 7] == 0)
        {
            results.push_back(nullptr);
            continue;
        }      
        int8_t* result = new int8_t;
        std::thread* worker = new std::thread(minimaxThreadCall,result, player ? i : i + 7, position, player, depth);
        workers.push_back(worker);
        results.push_back(result);
    }
    for (std::thread* i : workers)
    {
        i->join();
    }
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
    
    std::cout << "Evaluation: " << (player ? +score : +(-score)) << std::endl;

    workers.clear();
    results.clear();

    return bestIndex;
}

void print(unsigned char* position)
{
    for (int i = COMPUTER_SCORE; i > PLAYER_SCORE - 1; i--)
        std::cout << std::setw(3) << + position[i];
    std::cout << std::endl << "   ";
    for (int i = 0; i < PLAYER_SCORE; i++)
        std::cout << std::setw(3) << +position[i];
    std::cout << std::endl;
}

int main()
{
    uint8_t position[POSITION_LENGTH]
    {
        4,4,4,4,4,4,
        0,
        4,4,4,4,4,4,
        0
    };

    bool player = true;
    int cacheResult;
    int inputMove;
    std::string input;

    std::cout << "   < 0--1--2--3--4--5 >" << std::endl;
    print(position);

    while (!PlayerEmpty(position) && !ComputerEmpty(position))
    {
        if (!player)
        {
            cacheResult = minimaxRoot(position, player, 14);
            std::cout << "Calculated move: " << (player ? cacheResult : 12 - cacheResult) << std::endl;
            player = move(position, cacheResult, player);
        }
        else
        {
            std::cout << "Move:";
            std::cin >> input;

            inputMove = stoi(input);

            if (0 <= inputMove < 6 && position[inputMove] > 0)
                player = move(position, inputMove, player);
            else
                std::cout << "Invalid Input!" << std::endl;
        }
        std::cout << "   < 0--1--2--3--4--5 >" << std::endl;
        print(position);
        
        std::cout << " <----<---<-<>->--->---->" << std::endl;
    }

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
    std::cin.get();
}