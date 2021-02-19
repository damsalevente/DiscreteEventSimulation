#include "coffeesimulator.h"
#include "handler_functions.h"
elem cashierHandler[Idle + 1][SeatsAv + 1] = 
{
  {.nextState = Busy,
  .f = setBusyAndScheduleOrderEventAtRandom}
  {.nextState = Idle,
  .f = printok}
  {.nextState = Busy,
  .f = waitCasher},
  {.nextState = Idle,
  .f = printok}, //ne h�vjon meg erre semmit, nem az � dolga.

};
elem coffeeHandler[Idle + 1][SeatsAv + 1] = 
{
  {.nextState = Busy,
  .f = makeCoffee}
  {.nextState = Idle,
  .f = printok},
  {.nextState = Busy,
  .f = waitimbusy}, 
  {.nextState = Idle,
  .f = sitdown},
};
elem capacityHandler[Idle + 1][SeatsAv + 1]
{
  {.nextState = Idle
  .f = updateSeats},
  {.nextState = Busy,
  .f = scheduleSeatsAvEvent},
  {.nextState = Idle,
  .f = updateAvSeats},
  {.nextState = Idle,
  .f = updateSeats},
  {.nextState = Busy,
  .f = wait},
  {.nextState = Busy,
  .f = takeAway},
};