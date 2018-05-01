// coffeesimulator.cpp : main project file.
#include "stdafx.h"
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#define MAXEVENT 100000
#define MAX_COSTUMER 500
#define NUMCOFFEMACHINES 10
#define SEATS 50
using namespace System;

typedef enum state{Busy,Idle};
typedef enum eventType{CostIn,CostOut,Order,Stay, CoffeRdy,Full,SeatsAv
};
typedef struct {
	int costumerId;
	int startTime;
	int finishTime;
	int waitTime;
	int timeInSystem;
}statEl;

typedef struct {
	int guestN;
	int time;
}histEl;


typedef struct event_ {
	eventType type;
	int costumerId;
	int startTime;
	int arrive;	//mikor �rkezett az event
	int servTime;	//mennyi id� sz�ks�ges hozz�
	int handler;  //1 == k�v�, 2 == kassza,3==�l�hely
}event_;
//glob�lis v�ltoz�k
int systime=0;
event_ statBuffer[MAXEVENT];
event_ eventBuffer[MAXEVENT];	//even FIFO list�ja
int statSize = 0;
int first = 0;
int last = -1;
int eventSize=0;
int guests = 0;
int avCoffeMaker=NUMCOFFEMACHINES;
int kiosztva = 0;
bool full() {
	return eventSize == MAXEVENT;
}
bool empty() {
	return eventSize == 0;
}
void createEvent(eventType et,int costumerId, int arrive, int servTime, int handler,int startTime)
{
	if (!full())
	{
		if (last == MAXEVENT - 1)
		{
			int newStart = 0;
			for (int i = first; i < MAXEVENT; i++)
			{
				eventBuffer[newStart++] = eventBuffer[i];
			}
			last = eventSize-1;	//kezdjuk nullarol az elembeszurast.
			first = 0;
		}
		last++;
		eventBuffer[last].type = et;
		eventBuffer[last].arrive = arrive;
		eventBuffer[last].costumerId = costumerId;
		eventBuffer[last].handler = handler;
		eventBuffer[last].servTime = servTime;
		//eventBuffer[last].waitTime = 0;
		eventBuffer[last].startTime = startTime;
		eventSize++;
	}
}
void deleteEvent()
{
			event_ data = eventBuffer[first++];

			if (first == MAXEVENT) {
				first = 0;
			}
			eventSize--;
			statBuffer[statSize++] = data;

			
}

//type, vagyis, hogy kinek az �llapotg�p�hez adja.
event_* getNextEvent()
{
	if(!full())
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
//function declarations for cashier
void setBusyAndScheduleOrderEventAtRandom(event_ *ev)
{
	//guests++;
	createEvent(Order, ev->costumerId, systime + ev->servTime, 0, 2, systime + ev->servTime); //Order 1 perc m�lva kirakva,
	createEvent(Order,ev->costumerId,systime+ev->servTime,10+gen_exp(0.6),1,systime+ev->servTime);		//k�v� kapja az eventet, costumer id �s servTime-ja a k�v�nak 10 perc lesz.
		//ha acashier megkapja az ordert, azt jelenti hogy foglalkoztak vele
	printf("Prepare %d costumer's order event at %d\n",ev->costumerId,systime+ev->servTime);
	printf("Prepare %d costumer's coffee making at %d\n", ev->costumerId, systime + ev->servTime);
}
void waitCasher(event_ *ev) {
	createEvent(CostIn,ev->costumerId,ev->arrive,1,2,systime+1); 	//nem tudjuk, mennyit kell v�rni, hogy felszabaduljon a kassz�s, �jra beletessz�k a rendszerbe.												//startime-ot n�veljuk
	ev->handler = -1;																				////�s ezzel egy�tt a v�rakoz�si id� is n�vekedett, de ezt nem kell sz�mon tartani, el�g a starttime-arrivetime
	printf("Id:%d Wait until %d\n",ev->costumerId,systime+1);
}
void printok(event_ *ev)
{
	if(ev->type==Order)
		printf("Cashier ready\n");
	if (ev->type == CoffeRdy)
		printf("Coffemachine ready\n");
}
//end of function declarations for cashier
//coffee functions
//orderevent-re j�tt be, az ott jelenl�v� servTime lesz az ir�nyado
void makeCoffee(event_*ev){
	avCoffeMaker--;
	createEvent(CoffeRdy, ev->costumerId, systime + ev->servTime,1,1,systime+ev->servTime);
	printf("Order taken,coffee ready at %d\n", systime + ev->servTime);
	kiosztva = 1;
}
//makeCoffe volt az event
void sitdown(event_ *ev)
{
	createEvent(Stay, ev->costumerId, systime+ev->servTime, 20, 3,systime+ev->servTime);	// 20 percet tart�zkodik
	avCoffeMaker++;
	printf("I'm ready\n");
	kiosztva = 1;

}
void waitimbusy(event_ *ev) {
	printf("I'm busy\n");
}
void setFree(event_ *ev) {
	avCoffeMaker++;
	printf("Machine will be free for the next event\n");
}
//eof coffe functions
//start of capacity functions
void scheduleSeatsAvEvent(event_ *ev)
{
	printf("Costumer sit down, at %d he/she will leave\n", systime + ev->servTime);
	createEvent(SeatsAv, ev->costumerId, systime+ev->servTime, 15, 3, systime +ev->servTime);	//15 perc m�lva k�rdezi �jra
	guests++;
	printf("%d took the coffee and left, no available seats left for him\n", ev->costumerId);
}
void updateAvSeats(event_ *ev)
{
	guests++;
	printf("Someone took a seat: %d guest(s) in the coffe house\n", guests);
	createEvent(SeatsAv, ev->costumerId, systime + ev->servTime, 10, 3, systime + ev->servTime);
	if (guests == SEATS)
	{
		printf("Schedule full for the next event\n");
		createEvent(Full, ev->costumerId, systime + 1, 15, 3, systime + 1);	//k�vetkez� �temt�l foglalt, 50 percig lesz bent, ez kell a k�vetkez� schedule-nek
	}
}
void takeAway(event_ *ev)
{
	
	printf("A %d costumer elvitte\n", ev->costumerId);
}
void updateSeats(event_ *ev)
{
	guests--;
	printf("Costumer Id:%d went home\n,%d guest(s) remaining\n", ev->costumerId,guests);
}
//end of capacity functions 
//deletes first element;
event_ removeEvent()
{
	event_ ret = eventBuffer[first++];
	if (first == MAXEVENT)first = 0;
	eventSize--;
	return ret;
}
typedef struct elem {
	state nextState;
	void(*f) (event_*);
}elem;
elem cashierHandler[Idle + 1][ SeatsAv + 1];
elem coffeeHandler[Idle + 1][SeatsAv + 1];
elem capacityHandler[Idle + 1][SeatsAv + 1];

void initCoffeeHandler() {
	coffeeHandler[Idle][Order].nextState = Busy;
	coffeeHandler[Idle][Order].f = makeCoffee;
	coffeeHandler[Idle][CoffeRdy].nextState = Idle;
	coffeeHandler[Idle][CoffeRdy].f = printok;
	coffeeHandler[Busy][Order].nextState = Busy;
	coffeeHandler[Busy][Order].f = waitimbusy;	//ha nincs t�bb el�rhet� �s �n busy vagyok, akkor majd a f�program �temez, ha van, akkor �n b�k�n hagyom, m�s majd foglalkozik veled
	coffeeHandler[Busy][CoffeRdy].nextState = Idle;
	coffeeHandler[Busy][CoffeRdy].f = sitdown;
}
void initCashierHandler()
{
	cashierHandler[Idle][CostIn].nextState = Busy;
	cashierHandler[Idle][CostIn].f = setBusyAndScheduleOrderEventAtRandom;
	cashierHandler[Idle][Order].nextState = Idle;
	cashierHandler[Idle][Order].f = printok;
	cashierHandler[Busy][CostIn].nextState = Busy;	
	cashierHandler[Busy][CostIn].f = waitCasher;	
	cashierHandler[Busy][Order].nextState = Idle;
	cashierHandler[Busy][Order].f = printok;		//ne h�vjon meg erre semmit, nem az � dolga.
}
//ha tele van, akkor hazamegy
void initCapacityHandler()
{
	capacityHandler[Idle][SeatsAv].nextState = Idle;
	capacityHandler[Idle][SeatsAv].f = updateSeats;
	capacityHandler[Idle][Full].nextState = Busy;
	capacityHandler[Idle][Full].f = scheduleSeatsAvEvent;
	capacityHandler[Idle][Stay].nextState = Idle;
	capacityHandler[Idle][Stay].f = updateAvSeats;
	capacityHandler[Busy][SeatsAv].nextState = Idle;
	capacityHandler[Busy][SeatsAv].f = updateSeats;
	capacityHandler[Busy][Full].nextState = Busy;
	//capacityHandler[Busy][Full].f = takeAway;
	capacityHandler[Busy][Stay].nextState = Busy;
	capacityHandler[Busy][Stay].f = takeAway;
}

void generateEvents()
{
	printf("Running simulation with the following costumer set\n");
	srand(time(NULL));
	for (int i = 0; i < MAX_COSTUMER; i++)
	{
		int r = gen_exp(0.00138);	//arrive date, avarage arrive time=60min after opening, lambda = 1/average time 
		createEvent(CostIn,i,r,1,2,r);	// 1 perc felvenni a rendel�s�t
		printf("GeneratedId:%d arrival time:%d\n", i,r);
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
	int i,j;
	for (i = 0; i < eventSize - 1; i++)
	{
		for (j = 0; j <eventSize - i - 1; j++)
			if (eventBuffer[first+j].startTime > eventBuffer[first + j + 1].startTime)
				swap(&eventBuffer[first + j], &eventBuffer[first + j + 1]);
	}
}

void swapByType() {
	int i;
	for (i = first; i < (first+eventSize) - 1; i++)
		if (eventBuffer[i].arrive == eventBuffer[i + 1].arrive)	//ha ugyanakkor kezdodnek
			if (eventBuffer[i].type > eventBuffer[i + 1].type)			//es elobb van,ami blokkolni fog
				swap(&eventBuffer[i], &eventBuffer[i + 1]);				//akkor csereljuk meg oket
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
void stat(histEl hist [],int histNum)
{
	statEl Stat[MAX_COSTUMER];
	double averageWaitingTime = 0;
	double averageTimeInSystem = 0;
	FILE *fp = fopen("Stat.csv", "w");
	for (int i = 0; i < MAX_COSTUMER; i++)
		Stat[i].waitTime = 0;
	qsort((void*)statBuffer, statSize, sizeof(statBuffer[0]), byType);
	qsort((void*)statBuffer, statSize, sizeof(statBuffer[0]), byId);
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
	if (fp) {
		fprintf(fp,"Costumer Id;Start time;Finish Time;Wait time;Time in System\n");
		for (int i = 0; i < MAX_COSTUMER; i++)
			fprintf(fp, "%d;%d;%d;%d;%d\n", i, Stat[i].startTime, Stat[i].finishTime, Stat[i].waitTime, Stat[i].timeInSystem);
			fclose(fp);
	}
	fp = fopen("Hist.csv", "w");
	if (fp)
	{
		fprintf(fp,"Time;Number of Costumers\n");
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
	char *eventmassage[] = { "CostIn","CostOut","Order","Stay", "CoffeRdy","Full","SeatsAv" };
	FILE *fp = fopen("log.txt", "w");
	fprintf(fp, "Event type\tCostumer Id\tStart time\tServing Time\tArrive date\n");
	for (int i = 0; i < statSize; i++)
	{
		
		fprintf(fp, "%s\t%d\t%d\t%d\t%d\n",eventmassage[statBuffer[i].type], statBuffer[i].costumerId, statBuffer[i].startTime, statBuffer[i].servTime, statBuffer[i].arrive);
	}
}
int main()
{
	//init esem�nyqueue: mikor rendel, mennyi ideig rendel, k�vetkez� �llapot,
	//CostumerTimes: sum(el�z� esem�ny id�pontja-mostani esem�ny id�pontja
	//minden esem�nynek lesz egy �sszegzett ideje int tipus
	//
	generateEvents();
	//vez�rl�s init.
	initCashierHandler();
	initCoffeeHandler();
	initCapacityHandler();
	printf("%d\n\n\n",capacityHandler[Idle][Stay].nextState);
	histEl hist[MAXEVENT];	//minden systime tick-re megn�zi, hogy h�nyan voltak, ebb�l egy hisztogram k�sz�l
	int histNum = 0;
	elem e;
	state S = Idle;
	state cS [NUMCOFFEMACHINES];
	state cC = Idle;
	for (int i = 0; i < NUMCOFFEMACHINES; i++)
	{
		cS[i] = Idle;
	}
	
	while (!empty())//v�ge
	{

		 //type szerint sorbarendezi, �gy mindig a fontos eventek lesznek elol

		bubbleSort();	//sorbarendezve starttime szerint
		swapByType();
		event_* currentEvent = getNextEvent();
		if (!currentEvent) {
			printf("Hiba\n\n");
		}
		else {
			systime = currentEvent->startTime;
			printf("-------------->New Event<------------------------\n");
			printf("\nsystime:%d\n", systime);
			if(currentEvent->handler == 2){
			//kassz�s(ok) �llapotg�pe
				printf("Cashier Event\n");
				e = cashierHandler[S][currentEvent->type];
				e.f(currentEvent);//random timeal);//esemeny utan nem hasznalhato az event mutato, mert torlodik.
				S = e.nextState;
				int nosaveflag = 0;
				if (currentEvent->handler == -1)
				{
					nosaveflag = 1;	//waitevent
				}
				
				deleteEvent();
				if (nosaveflag)
				{
					statSize--;	//meg kell n�zni debugban hogy j�l m�k�dik-e 
				}
			}
			if (currentEvent->handler == 3)
			{
				printf("Seat event\n");
				e = capacityHandler[cC][currentEvent->type];
				e.f(currentEvent);
				cC = e.nextState;
				deleteEvent();

			}
			if (currentEvent->handler == 1)
			{
				printf("CoffeEvent\n");
				if (avCoffeMaker == 0 && currentEvent->type==Order)
				{
					createEvent(currentEvent->type, currentEvent->costumerId, currentEvent->arrive, currentEvent->servTime, currentEvent->handler, currentEvent->startTime + 1);
					currentEvent->handler = -1;
					printf("Every coffee machine is busy,please wait\n");
					currentEvent->handler = -1;
				}
				for (int i = 0; i < NUMCOFFEMACHINES; i++) {
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
					nosaveflag = 1;	//waitevent
				}
				deleteEvent();
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
	stat(hist,histNum);
	log();
    return 0;
}


