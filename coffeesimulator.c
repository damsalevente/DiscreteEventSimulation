// coffeesimulator.cpp : main project file.
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include "coffeesimulator.h"
#define MAXEVENT 100000
#define MAX_COSTUMER 500
#define NUMCOFFEMACHINES 10
#define SEATS 50
static int systime=0;
event_ statBuffer[MAXEVENT];
event_ eventBuffer[MAXEVENT];	//even FIFO list�ja
static int statSize = 0;
static int first = 0;
static int last = -1;
static int eventSize=0;
static int guests = 0;
static int kiosztva = 0;
static bool full() {
  return eventSize == MAXEVENT;
}
static bool empty() {
  return eventSize == 0;
}

static int Buffer_HandleInsert(int *idx)
{
  if (full())
  {
    return 1;
  }
  if (last == MAXEVENT - 1)
  {
    int newStart = 0;
    for (int i = first; i < MAXEVENT; i++)
    {
      eventBuffer[newStart++] = eventBuffer[i];
    }
    last = eventSize - 1; //kezdjuk nullarol az elembeszurast.
    first = 0;
  }
  last++;
  *idx = last;
  return 0;
}
static void Event_Add(eventType et, int costumerId, int arrive, int servTime, int handler, int startTime)
{
  int idx = 0;
  if( 0 == Buffer_HandleInsert(&idx))
  {
    eventBuffer[last] = {.type = et, .arrive = arrive, .costumerId = costumerId, .handler = handler, .servTime = servTime,.startTime = startTime};
    eventSize++;
  }
}

static void Event_Remove()
{
  event_ data = eventBuffer[first++];
  first = (first == MAXEVENT)? 0 : first;
  eventSize--;
  statBuffer[statSize++] = data;
}

//type, vagyis, hogy kinek az �llapotg�p�hez adja.
event_ *getNextEvent()
{
  if (!full())
    return &eventBuffer[first];
  return 0;
}
//generate a random int, with mean=1/lambda
int gen_exp(double lambda)
{
  double u = (double)rand() / (RAND_MAX);
  //printf("Random number: %lf\n", u);
  return (int)(-log(1 - u) / lambda);
}

//end of capacity functions
//deletes first element;
event_ removeEvent()
{
  event_ ret = eventBuffer[first++];
  if (first == MAXEVENT)
    first = 0;
  eventSize--;
  return ret;
}


void generateEvents()
{
  printf("Running simulation with the following costumer set\n");
  srand(time(NULL));
  for (int i = 0; i < MAX_COSTUMER; i++)
  {
    int r = gen_exp(0.00138);           //arrive date, avarage arrive time=60min after opening, lambda = 1/average time
    Event_Add(CostIn, i, r, 1, 2, r); // 1 perc felvenni a rendel�s�t
    printf("GeneratedId:%d arrival time:%d\n", i, r);
  }
}

void swap(event_ *xp, event_ *yp)
{
  event_ temp = *xp;
  *xp = *yp;
  *yp = temp;
}

// A function to implement bubble sort
void bubbleSort()
{
  int i, j;
  for (i = 0; i < eventSize - 1; i++)
  {
    for (j = 0; j < eventSize - i - 1; j++)
      if (eventBuffer[first + j].startTime > eventBuffer[first + j + 1].startTime)
        swap(&eventBuffer[first + j], &eventBuffer[first + j + 1]);
  }
}

void swapByType()
{
  int i;
  for (i = first; i < (first + eventSize) - 1; i++)
    if (eventBuffer[i].arrive == eventBuffer[i + 1].arrive) //ha ugyanakkor kezdodnek
      if (eventBuffer[i].type > eventBuffer[i + 1].type)    //es elobb van,ami blokkolni fog
        swap(&eventBuffer[i], &eventBuffer[i + 1]);         //akkor csereljuk meg oket
}
int byId(const void *p, const void *q)
{
  int l = ((struct event_ *)p)->costumerId;
  int r = ((struct event_ *)q)->costumerId;
  return (l - r);
}
int byType(const void *p, const void *q)
{
  int l = ((struct event_ *)p)->type;
  int r = ((struct event_ *)q)->type;
  return (l - r);
}
void stat(histEl hist[], int histNum)
{
  statEl Stat[MAX_COSTUMER];
  double averageWaitingTime = 0;
  double averageTimeInSystem = 0;
  FILE *fp = fopen("Stat.csv", "w");
  for (int i = 0; i < MAX_COSTUMER; i++)
    Stat[i].waitTime = 0;
  qsort((void *)statBuffer, statSize, sizeof(statBuffer[0]), byType);
  qsort((void *)statBuffer, statSize, sizeof(statBuffer[0]), byId);
  for (int i = 0; i < statSize; i++)
  {
    Stat[statBuffer[i].costumerId].waitTime += (statBuffer[i].startTime - statBuffer[i].arrive);
    if (statBuffer[i].type == CostIn)
      Stat[statBuffer[i].costumerId].startTime = statBuffer[i].arrive;

    if (statBuffer[i].type == SeatsAv)
      Stat[statBuffer[i].costumerId].finishTime = statBuffer[i].arrive;
  }
  for (int i = 0; i < MAX_COSTUMER; i++)
    Stat[i].timeInSystem = Stat[i].finishTime - Stat[i].startTime;
  if (fp)
  {
    fprintf(fp, "Costumer Id;Start time;Finish Time;Wait time;Time in System\n");
    for (int i = 0; i < MAX_COSTUMER; i++)
      fprintf(fp, "%d;%d;%d;%d;%d\n", i, Stat[i].startTime, Stat[i].finishTime, Stat[i].waitTime, Stat[i].timeInSystem);
    fclose(fp);
  }
  fp = fopen("Hist.csv", "w");
  if (fp)
  {
    fprintf(fp, "Time;Number of Costumers\n");
    for (int i = 0; i < histNum; i++)
      fprintf(fp, "%d;%d\n", hist[i].time, hist[i].guestN);
  }
  fclose(fp);
  for (int i = 0; i < MAX_COSTUMER; i++)
  {
    averageWaitingTime += Stat[i].waitTime;
    averageTimeInSystem += Stat[i].timeInSystem;
  }
  printf("Average waiting time:%lf\nAverage Time in System:%lf\n", averageWaitingTime / MAX_COSTUMER, averageTimeInSystem / MAX_COSTUMER);
}
void log()
{
  char *eventmassage[] = {"CostIn", "CostOut", "Order", "Stay", "CoffeRdy", "Full", "SeatsAv"};
  FILE *fp = fopen("log.txt", "w");
  fprintf(fp, "Event type\tCostumer Id\tStart time\tServing Time\tArrive date\n");
  for (int i = 0; i < statSize; i++)
  {

    fprintf(fp, "%s\t%d\t%d\t%d\t%d\n", eventmassage[statBuffer[i].type], statBuffer[i].costumerId, statBuffer[i].startTime, statBuffer[i].servTime, statBuffer[i].arrive);
  }
}
void CoffeSimulation_GenerateEvents()
{
  generateEvents();
}

void Diagnostic_Start(){

  histEl hist[MAXEVENT];
  int histNum = 0;
}
void CoffeSimulation_Cyclic()
{
  while(!EventBuffer_Empty())
  {
    event_ e = EventBuffer_GetNextEvent();
    EventHandler_Step(e);
  }
}
void CoffeeSimulation_Start()
{
  CoffeeSimulation_Reset();
  CoffeSimulation_GenerateEvents(); /* initial costumer events */
  Diagnostic_Start();
  CoffeSimulation_Cyclic();

}
int main()
{
  CoffeeSimulation_Start();
  elem e;
  state S = Idle;
  state cS[NUMCOFFEMACHINES];
  state cC = Idle;
  for (int i = 0; i < NUMCOFFEMACHINES; i++)
  {
    cS[i] = Idle;
  }

  while (!empty()) //v�ge
  {

    //type szerint sorbarendezi, �gy mindig a fontos eventek lesznek elol

    bubbleSort(); //sorbarendezve starttime szerint
    swapByType();
    event_ *currentEvent = getNextEvent();
    if (!currentEvent)
    {
      printf("Hiba\n\n");
    }
    else
    {
      systime = currentEvent->startTime;
      printf("-------------->New Event<------------------------\n");
      printf("\nsystime:%d\n", systime);
      if (currentEvent->handler == 2)
      {
        //kassz�s(ok) �llapotg�pe
        printf("Cashier Event\n");
        e = cashierHandler[S][currentEvent->type];
        e.f(currentEvent); 
        S = e.nextState;
        int nosaveflag = 0;
        if (currentEvent->handler == -1)
        {
          nosaveflag = 1; 
        }
        Event_Remove();
        if (nosaveflag)
        {
          statSize--; 
        }
      }
      if (currentEvent->handler == 3)
      {
        printf("Seat event\n");
        e = capacityHandler[cC][currentEvent->type];
        e.f(currentEvent);
        cC = e.nextState;
        Event_Remove();
      }
      if (currentEvent->handler == 1)
      {
        printf("CoffeEvent\n");
        if (avCoffeMaker == 0 && currentEvent->type == Order)
        {
          Event_Add(currentEvent->type, currentEvent->costumerId, currentEvent->arrive, currentEvent->servTime, currentEvent->handler, currentEvent->startTime + 1);
          currentEvent->handler = -1;
          printf("Every coffee machine is busy,please wait\n");
          currentEvent->handler = -1;
        }
        for (int i = 0; i < NUMCOFFEMACHINES; i++)
        {
          if (!kiosztva)
          {
            e = coffeeHandler[cS[i]][currentEvent->type];
            e.f(currentEvent);
            cS[i] = e.nextState;
          }
        }
        kiosztva = 0;

        int nosaveflag = 0;
        if (currentEvent->handler == -1)
        {
          nosaveflag = 1; //waitevent
        }
        Event_Remove();
        if (nosaveflag)
        {
          statSize--;
        }
      }
    }
    //k�v�f�z�(k) �llapotg�pe
    //orderben az lesz benne, hogy mennyi ido elkesziteni
    hist[histNum].guestN = guests;
    hist[histNum].time = systime;
    histNum++;
  }
  stat(hist, histNum);
  log();
  return 0;
}
