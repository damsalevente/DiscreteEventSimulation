#include "coffeesimulator.h"

extern int systime;
//function declarations for cashier
void setBusyAndScheduleOrderEventAtRandom(event_ *ev)
{
  //guests++;
  Event_Add(Order, ev->costumerId, systime + ev->servTime, 0, 2, systime + ev->servTime);                 //Order 1 perc m�lva kirakva,
  Event_Add(Order, ev->costumerId, systime + ev->servTime, 10 + gen_exp(0.6), 1, systime + ev->servTime); //k�v� kapja az eventet, costumer id �s servTime-ja a k�v�nak 10 perc lesz.
  //ha acashier megkapja az ordert, azt jelenti hogy foglalkoztak vele
  printf("Prepare %d costumer's order event at %d\n", ev->costumerId, systime + ev->servTime);
  printf("Prepare %d costumer's coffee making at %d\n", ev->costumerId, systime + ev->servTime);
}
void waitCasher(event_ *ev)
{
  Event_Add(CostIn, ev->costumerId, ev->arrive, 1, 2, systime + 1); //nem tudjuk, mennyit kell v�rni, hogy felszabaduljon a kassz�s, �jra beletessz�k a rendszerbe.												//startime-ot n�veljuk
  ev->handler = -1;                                                   ////�s ezzel egy�tt a v�rakoz�si id� is n�vekedett, de ezt nem kell sz�mon tartani, el�g a starttime-arrivetime
  printf("Id:%d Wait until %d\n", ev->costumerId, systime + 1);
}
void printok(event_ *ev)
{
  if (ev->type == Order)
    printf("Cashier ready\n");
  if (ev->type == CoffeRdy)
    printf("Coffemachine ready\n");
}
//end of function declarations for cashier
//coffee functions
//orderevent-re j�tt be, az ott jelenl�v� servTime lesz az ir�nyado
void makeCoffee(event_ *ev)
{
  avCoffeMaker--;
  Event_Add(CoffeRdy, ev->costumerId, systime + ev->servTime, 1, 1, systime + ev->servTime);
  printf("Order taken,coffee ready at %d\n", systime + ev->servTime);
  kiosztva = 1;
}
//makeCoffe volt az event
void sitdown(event_ *ev)
{
  Event_Add(Stay, ev->costumerId, systime + ev->servTime, 20, 3, systime + ev->servTime); // 20 percet tart�zkodik
  avCoffeMaker++;
  printf("I'm ready\n");
  kiosztva = 1;
}
void waitimbusy(event_ *ev)
{
  printf("I'm busy\n");
}
void setFree(event_ *ev)
{
  avCoffeMaker++;
  printf("Machine will be free for the next event\n");
}
//eof coffe functions
//start of capacity functions
void scheduleSeatsAvEvent(event_ *ev)
{
  printf("Costumer sit down, at %d he/she will leave\n", systime + ev->servTime);
  Event_Add(SeatsAv, ev->costumerId, systime + ev->servTime, 15, 3, systime + ev->servTime); //15 perc m�lva k�rdezi �jra
  guests++;
  printf("%d took the coffee and left, no available seats left for him\n", ev->costumerId);
}
void updateAvSeats(event_ *ev)
{
  guests++;
  printf("Someone took a seat: %d guest(s) in the coffe house\n", guests);
  Event_Add(SeatsAv, ev->costumerId, systime + ev->servTime, 10, 3, systime + ev->servTime);
  if (guests == SEATS)
  {
    printf("Schedule full for the next event\n");
    Event_Add(Full, ev->costumerId, systime + 1, 15, 3, systime + 1); //k�vetkez� �temt�l foglalt, 50 percig lesz bent, ez kell a k�vetkez� schedule-nek
  }
}
void takeAway(event_ *ev)
{

  printf("A %d costumer elvitte\n", ev->costumerId);
}
void updateSeats(event_ *ev)
{
  guests--;
  printf("Costumer Id:%d went home\n,%d guest(s) remaining\n", ev->costumerId, guests);
}