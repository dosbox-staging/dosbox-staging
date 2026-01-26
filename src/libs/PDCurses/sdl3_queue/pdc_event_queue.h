#ifndef PDC_EVENT_QUEUE_H
#define PDC_EVENT_QUEUE_H

#include <queue>
#include <string>
#include <SDL3/SDL.h>

struct QueuedEvent {
    SDL_Event ev = {};
    std::string text = {}; /* copied text for SDL_EVENT_TEXT_INPUT */
};

extern std::queue<QueuedEvent> debugger_event_queue;

#endif // PDC_EVENT_QUEUE_H