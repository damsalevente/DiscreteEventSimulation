#include "coffee.h"
#define NUMCOFFEMACHINES 100

typedef struct coffee_machine{
    int available;
    int state;
    int fault;
    int *handler;
}coffee_machine;


static coffee_machine CoffeMaker[NUMCOFFEMACHINES];

Coffee_Reserve(int *hndlr)
{
    for( int i = 0; i <NUMCOFFEMACHINES; i++)
    {
        if(CoffeMaker[i].available && CoffeMaker[i].fault == 0)
        {
            CoffeMaker[i].available = false;
            CoffeMaker.handler = hndlr;
        }
    }
}
Coffee_Free(int *hndlr)
{
    for(int i = 0; i < NUMCOFFEMACHINES; i++)
    {
        if(CoffeMaker[i].handler != NULL && CoffeMaker[i].handler == *hndlr)
        {
            CoffeMaker[i].handler = NULL;
            CoffeMaker[i].available = 1;
        }
    }
}

Coffee_HandleEvent()
{
    /* setstate with the coffehandler stuff  */
}



int Coffee_GetAvailable()
{
    return avCoffeMaker;
}