#ifndef DES_TYPES_H
#define DES_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#define DES_MAX_EVENTS       100000
#define DES_MAX_ENTITIES     10000
#define DES_MAX_RESOURCES    64
#define DES_MAX_STAGES       64
#define DES_MAX_EVENT_TYPES  32
#define DES_MAX_STATES       16
#define DES_MAX_OUTCOMES     8
#define DES_MAX_NAME         64
#define DES_MAX_ARRIVALS     16
#define DES_INVALID_ID       (-1)

typedef enum {
    DES_OK = 0,
    DES_ERR_QUEUE_FULL,
    DES_ERR_QUEUE_EMPTY,
    DES_ERR_NULL_POINTER,
    DES_ERR_FILE_IO,
    DES_ERR_CONFIG,
    DES_ERR_NO_RESOURCE,
    DES_ERR_NO_STAGE,
    DES_ERR_NO_OUTCOME,
    DES_ERR_OUT_OF_MEMORY,
    DES_ERR_ENTITY_FULL,
} DesErrorCode;

const char *DesError_toString(DesErrorCode code);

typedef enum {
    DES_ACTION_NONE = 0,
    DES_ACTION_ACQUIRE_AND_PROCESS,
    DES_ACTION_RELEASE_AND_DISPATCH,
    DES_ACTION_RELEASE_AND_RETRY,
    DES_ACTION_WAIT_RETRY,
    DES_ACTION_ENTITY_ENTER,
    DES_ACTION_ENTITY_EXIT,
    DES_ACTION_CUSTOM,
} DesActionType;

typedef enum {
    DES_DIST_FIXED = 0,
    DES_DIST_UNIFORM,
    DES_DIST_EXPONENTIAL,
    DES_DIST_NORMAL,
} DesDistType;

typedef struct {
    DesDistType type;
    double      param1;
    double      param2;
} DesDistribution;

typedef struct {
    int          state_index;
    int          event_index;
    int          next_state_index;
    DesActionType action_type;
    int          custom_action_id;
} DesFsmTransition;

typedef struct {
    char   name[DES_MAX_NAME];
    double probability;
    int    next_stage_id;
    int    next_event_index;
} DesStageOutcome;

typedef struct {
    char  name[DES_MAX_NAME];
    int   resource_type_id;
    int   initial_state_index;
    int   num_states;
    char  state_names[DES_MAX_STATES][DES_MAX_NAME];
    int   num_event_types;
    char  event_type_names[DES_MAX_EVENT_TYPES][DES_MAX_NAME];
    int   num_transitions;
    DesFsmTransition transitions[DES_MAX_STATES * DES_MAX_EVENT_TYPES];
    DesDistribution  processing_time;
    int              num_outcomes;
    DesStageOutcome  outcomes[DES_MAX_OUTCOMES];
} DesStageDef;

typedef struct {
    char name[DES_MAX_NAME];
    int  count;
    int  available_at;
} DesResourceDef;

typedef struct {
    char            name[DES_MAX_NAME];
    int             entity_count;
    int             entry_stage_id;
    int             start_time;
    int             priority;
    DesDistribution inter_arrival;
} DesEntityArrival;

typedef struct {
    bool  record_events;
    bool  record_entity_flow;
    bool  record_resource_util;
    char  output_dir[256];
} DesStatsConfig;

#define DES_MAX_VALIDATION_ERRORS 32
#define DES_MAX_ERROR_MESSAGE     256

typedef struct {
    int  num_errors;
    char errors[DES_MAX_VALIDATION_ERRORS][DES_MAX_ERROR_MESSAGE];
} DesValidationResult;

typedef struct {
    char            name[DES_MAX_NAME];
    int             num_resources;
    DesResourceDef  resources[DES_MAX_RESOURCES];
    int             num_stages;
    DesStageDef     stages[DES_MAX_STAGES];
    int             num_arrivals;
    DesEntityArrival arrivals[DES_MAX_ARRIVALS];
    int             max_time;
    int             max_events;
    int             entity_capacity;
    unsigned int    seed;
    DesStatsConfig  stats;
    char            last_error[256];
} DesSimConfig;

typedef struct {
    int  id;
    int  target_stage_id;
    int  event_type;
    int  entity_id;
    int  time;
    int  priority;
    int  data[4];
} DesEvent;

typedef struct {
    int   id;
    int   current_stage_id;
    int   current_state;
    int   entry_time;
    int   stage_entry_time;
    int   completion_time;
    int   outcome_id;
    bool  active;
    int   num_stage_visits;
    void *user_data;
} DesEntity;

typedef struct {
    int  type_id;
    int  instance_id;
    int  current_state;
    int  assigned_entity;
    int  assigned_stage;
    int  available_at_time;
} DesResourceInstance;

typedef struct {
    int          next_state;
    DesActionType action_type;
    int          custom_action_id;
    bool         defined;
} DesFsmEntry;

typedef struct {
    int           id;
    char          name[DES_MAX_NAME];
    int           resource_type_id;
    int           initial_state;
    int           num_states;
    int           num_event_types;
    DesFsmEntry  *fsm;
    DesDistribution processing_time;
    int              num_outcomes;
    DesStageOutcome  outcomes[DES_MAX_OUTCOMES];
    int           current_state;
    int           entry_event_type;
    int           completion_event_type;
} DesStage;

typedef struct {
    int  id;
    char name[DES_MAX_NAME];
    int  count;
    int  available;
    int  first_instance_idx;
    int  instance_count;
} DesResourceType;

typedef void (*DesActionFn)(void *engine, const DesEvent *event, void *user_data);

typedef struct {
    char        name[DES_MAX_NAME];
    DesActionFn fn;
    void       *user_data;
} DesCustomAction;

typedef struct {
    int entity_id;
    int stage_id;
    int enter_time;
    int exit_time;
    int outcome_id;
} DesEntityRecord;

typedef struct {
    int time;
    int resource_type_id;
    int instance_id;
    int state;
    int entity_id;
} DesResourceRecord;

typedef struct {
    int           time;
    int           event_id;
    int           entity_id;
    int           stage_id;
    int           event_type;
    int           from_state;
    int           to_state;
    DesActionType action_type;
    int           resource_type_id;
    int           resource_instance_id;
    int           outcome_id;
    bool          accepted;
} DesTransitionRecord;

typedef struct {
    DesEntityRecord   *entity_records;
    int                num_entity_records;
    int                entity_record_capacity;
    DesResourceRecord *resource_records;
    int                num_resource_records;
    int                resource_record_capacity;
    DesTransitionRecord *transition_records;
    int                  num_transition_records;
    int                  transition_record_capacity;
} DesStatsCollector;

typedef struct {
    DesEvent *buffer;
    int       capacity;
    int       first;
    int       last;
    int       size;
} DesEventQueue;

typedef struct DesEngine DesEngine;

#endif
