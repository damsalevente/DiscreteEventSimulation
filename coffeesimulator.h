
typedef enum state
{
  Busy,
  Idle,
  NUM_STATES
};
typedef enum eventType
{
  CostIn,
  CostOut,
  Order,
  Stay,
  CoffeRdy,
  Full,
  SeatsAv,
  NUM_EVENTS
};
typedef struct
{
  int costumerId;
  int startTime;
  int finishTime;
  int waitTime;
  int timeInSystem;
} statEl;

typedef struct
{
  int guestN;
  int time;
} histEl;

typedef struct event_
{
  eventType type;
  int costumerId;
  int startTime;
  int arrive;   //mikor �rkezett az event
  int servTime; //mennyi id� sz�ks�ges hozz�
  int handler;  //1 == k�v�, 2 == kassza,3==�l�hely
} event_;
typedef struct elem
{
  state nextState;
  void (*f)(event_ *);
} elem;