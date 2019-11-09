#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <random>
#include <iterator>

extern "C"
{
#include <SDL2/SDL.h>
#include <SDL2/SDL_gpu.h>
}

// Contains local static data
namespace
{
    // window variables
    static GPU_Target *target = nullptr;
    static bool shouldExit = false;

    constexpr unsigned short width  = 1200;
    constexpr unsigned short height = 900;

    // Each cell will have one of these states
    enum class CellState
    {
        Nothing,
        Snake,
        SnakeHead,

        GreenPellet,
        RedPellet
    };

    // Direction of the ::player/snake
    enum class SnakeDirection : unsigned
    {
        Up,
        Down,
        Left,
        Right,

        None
    };

    struct coord
    {
        int x;
        int y;

        bool operator==(coord other)
        {
            if((this->x == other.x) && (this->y == other.y))
            {
                return true;
            }
            return false;
        }
    };

    // Struct represnting the ::player.
    struct Snake
    {
        ::SnakeDirection currentDirection = ::SnakeDirection::None;
        unsigned snakeLength              = 0;
        unsigned numTurnsLeft             = 0;
        unsigned points                   = 0;
        std::vector<::coord> allCoords;        // coordinates of all snake body parts
    };

    struct Pellet
    {
        ::coord co;
        ::CellState pelletType;
    };

    // game constructs and data
    constexpr unsigned numRows = 25;
    constexpr unsigned numCols = 25;

    static Pellet greenPel = { {0, 0}, ::CellState::GreenPellet };
    static Pellet redPel = { {0, 0}, ::CellState::RedPellet };

    static bool greenPelletExists = false;
    
    static std::vector<std::vector<::CellState>> cells (numRows,
                                                        std::vector<::CellState>
                                                        (numCols,
                                                         ::CellState::Nothing));

    static Snake player;

    static bool IsCollideWithSnake(coord other)
    {
        return false;
    }
    
    static void CreatePellets()
    {
        
        std::mt19937 randomGenerator(std::chrono::high_resolution_clock
                                     ::now()
                                     .time_since_epoch()
                                     .count());
        
        std::uniform_int_distribution<unsigned> dis(0, numRows);
        
        auto makePellet = [&](::CellState type)
                          {
                              bool madePellet = false;
                              ::Pellet &curPel = (type == ::CellState::GreenPellet)
                                    ? ::greenPel : ::redPel;

                              while(!madePellet)
                              {
                                  for(unsigned i = 0; i < numRows; i++)
                                  {
                                      for(unsigned j = 0; j < numCols; j++)
                                      {
                                          ::CellState &curCell = cells[i][j];
                                          if((dis(randomGenerator) == 0)
                                             && (curCell == ::CellState::Nothing)) // make sure nothing is on
                                              //this particular cell
                                          {
                                              curCell = type;
                                              madePellet = true;
                                              curPel.co = { static_cast<int>(i), static_cast<int>(j) };
                                              return;
                                          }
                                      }
                                  }
                              }
                              
                          };

        makePellet(::CellState::GreenPellet);
        makePellet(::CellState::RedPellet);

        ::greenPelletExists = true;
    }

    static bool Init()
    {
        // init sdl
        if(SDL_Init(SDL_INIT_EVENTS) != 0)
        {
            std::cerr << "SDL: " << SDL_GetError() << '\n';
            return false;
        }

        // init sdl_gpu
        target = GPU_Init(::width, ::height, GPU_DEFAULT_INIT_FLAGS);
        
        if(target == nullptr)
        {
            std::cerr << "GPU: Unable to initialize" << '\n';
            return false;
        }

        // init game constructs

        ::CreatePellets();

        auto initSnake = [&]()
                         {
                             
                             bool hasMadeSnake = false;

                             std::mt19937 randomGenerator(std
                                                          ::chrono
                                                          ::high_resolution_clock
                                                          ::now()
                                                          .time_since_epoch()
                                                          .count());
        
                             std::uniform_int_distribution<unsigned> colDis(0,
                                                                       numCols);

                             std::uniform_int_distribution<unsigned> rowDis(0,
                                                                            numRows);
                             while(!hasMadeSnake)
                             {
                                 for(unsigned i = 0; i < numRows; i++)
                                 {
                                     if(rowDis(randomGenerator) == 0)
                                     {
                                         for(unsigned j = 0; j < numCols; j++)
                                         {
                                             ::CellState &curCell = cells[i][j];
                                             // also check to make sure the snake
                                             // doesn't spawn on a pellet
                                             if((colDis(randomGenerator) == 0)
                                                && ((curCell != ::CellState::RedPellet)
                                                    && (curCell != ::CellState::GreenPellet)))
                                             {
                                                 curCell = ::CellState::SnakeHead;
                                                 ::player.allCoords
                                                       .push_back({ static_cast<int>(i),
                                                                    static_cast<int>(j) });
                                                 hasMadeSnake = true;
                                                 return;
                                             }
                                         }
                                     }
                                 }
                             }
                         };

        initSnake();
        
        return true;
    }

    /*
     * 
     */
    static inline void GetKbdInput(SDL_Event &event)
    {
        switch(event.key.keysym.sym)
        {
        case SDLK_UP:
        case SDLK_w:
            ::player.currentDirection = ::SnakeDirection::Up;
            break;
            
        case SDLK_DOWN:
        case SDLK_s:
            ::player.currentDirection = ::SnakeDirection::Down;
            break;

        case SDLK_LEFT:
        case SDLK_a:
            ::player.currentDirection = ::SnakeDirection::Left;
            break;

        case SDLK_RIGHT:
        case SDLK_d:
            ::player.currentDirection = ::SnakeDirection::Right;
            break;

        case SDLK_ESCAPE:
            // Quit menu TODO
            break;

        default:
            break;
        }
    }

    /*
     * Gets SDL input
     */
    static inline void GetInput()
    {
        SDL_Event event;

        // TODO
        while(SDL_PollEvent(&event))
        {
            switch(event.type)
            {
                // keyboard input
            case SDL_KEYDOWN:
                GetKbdInput(event);
                break;

                // Exit
            case SDL_QUIT:
                shouldExit = true;
                break;

            default:
                break;
            }
        }

    }

    /*
     * Kill the snake and exit the game.
     */
    static void Die()
    {
        // TODO
        exit(0);
    }
    
    /*
     * The game simulation.
     */
    static inline void Simulate()
    {
        auto &headCoord = ::player.allCoords[0];// 0 will always be the head

        // check that the snake head isn't touching the snake body, if so,
        // kill the game
        for (unsigned i = 1; i < ::player.allCoords.size(); i++)
        {
            if(::player.allCoords[i] == headCoord)
            {
                std::cout << "The player is dead!\n";
                ::Die();
            }
        }

        // check if the snake head is touching any pellets
        if(headCoord == ::greenPel.co)
        {
            // TODO
        }
        else if(headCoord == ::redPel.co)
        {
            // TODO
        }

        switch(::player.currentDirection)
        {
        case ::SnakeDirection::Up:
            // move up
            headCoord.y--;
            if(headCoord.y == -1)
            {
                headCoord.y = numCols - 1;
            }
            break;

        case ::SnakeDirection::Down:
            headCoord.y++;
            if(headCoord.y == numCols)
            {
                headCoord.y = 0;
            }
            break;
            
        case ::SnakeDirection::Left:
            headCoord.x--;
            if(headCoord.x == -1)
            {
                headCoord.x = numRows - 1;
            }
            break;
            
        case ::SnakeDirection::Right:
            headCoord.x++;
            if(headCoord.x == numRows)
            {
                headCoord.x = 0;
            }
            break;
            
        default: // no direction
            break;
        }

        std::cout << "Made it...\n";
        // change snake coords in the cells
        cells[headCoord.x][headCoord.y] = ::CellState::SnakeHead;
        for(int i = 0; i < numRows; i++)
        {
            for(int j = 0; j < numCols; j++)
            {
                if(cells[i][j] == ::CellState::SnakeHead)
                {
                    //cells[i][j]
                }
            }
        }
        
        // init pellets if they have already been eaten
        if(!::greenPelletExists)
        {
            CreatePellets();
        }

    }

    /*
     * Renders the cells to the screen.
     */
    static inline void Draw()
    {
        GPU_Clear(target);

        GPU_Rect drawCell = GPU_MakeRect(0.0F, 0.0F,
                                         static_cast<float>(width / numRows),
                                         static_cast<float>(height / numCols));
        
        SDL_Color color;

        for(unsigned i = 0; i < numRows; i++)
        {
            for(unsigned j = 0; j < numCols; j++)
            {
                ::CellState &cell = cells[i][j];

                drawCell.x = static_cast<float>(i * (width / numRows));
                drawCell.y = static_cast<float>(j * (height / numCols));
                
                switch(cell)
                {
                    // the overhwelming majority of cases
                case ::CellState::Nothing:
                    color = { 128, 128, 128, 0xFF };
                    break;

                case ::CellState::Snake:
                case ::CellState::SnakeHead:
                    color = { 34, 139, 34, 0xFF };
                    break;


                case ::CellState::GreenPellet:
                    color = { 0, 0xFF, 0, 0xFF  };
                    break;
                    
               case ::CellState::RedPellet:
                   color = { 0xFF, 0, 0, 0xFF  };
                   break;
                }

                // main rectangle
                GPU_RectangleFilled2(target, drawCell, color);
                // outline
                GPU_Rectangle2(target, drawCell, { 0, 0, 0, 0xFF });
            }

        }

        GPU_Flip(target);
    }

    static void Run()
    {
        bool run = true;

        namespace chron = std::chrono;
        using clock = chron::high_resolution_clock;

        clock::time_point startFrameTime;
        chron::duration<float> frameRenderTime;

        constexpr float frameTime = 1000.0F / 15.0F; // 60 fps

        
        while(run)
        {
            // start framerate control
            startFrameTime = clock::now();

            // input
            ::GetInput();

            if(shouldExit)
            {
                run = false;
            }

            // simulate
            ::Simulate();
            

            // render
            ::Draw();
            

            // end framerate control
            frameRenderTime = clock::now() - startFrameTime;
            if(frameTime > frameRenderTime.count())
            {
                chron::duration<double, std::milli> waitTimeMS
                    (frameTime - frameRenderTime.count());
                chron::milliseconds deltaDuration = chron::duration_cast<chron::milliseconds>
                    (waitTimeMS);
                std::this_thread::sleep_for(deltaDuration);
            }
        }
    }

    static void End()
    {
        GPU_FreeTarget(target);
        target = nullptr;
        GPU_Quit();
        SDL_Quit();
    }
}

int main(int argc, char *argv[])
{
    if(!::Init())
    {
        ::End();
        return -1;
    }

    ::Run();

    ::End();
    return 0;
}
