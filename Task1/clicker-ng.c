#include <stdio.h>
#include <string.h>
#include "contiki.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "sys/clock.h"

/*---------------------------------------------------------------------------*/
#define MAX_NUMBER_OF_EVENTS 3
#define EVENT_TIMEOUT (30 * CLOCK_SECOND)

struct event
{
  clock_time_t time;
  linkaddr_t addr;
};

static struct event event_history[MAX_NUMBER_OF_EVENTS];
static struct etimer check_timer;

/*---------------------------------------------------------------------------*/
// This function manages the logic of counting distinct nodes
void handle_event(const linkaddr_t *src)
{
  clock_time_t now = clock_time();
  int i, oldest = 0;
  bool exists = false;

  // 1. Only update the list if we have a real address (not NULL)
  if (src != NULL && !linkaddr_cmp(src, &linkaddr_null))
  {
    for (i = 0; i < MAX_NUMBER_OF_EVENTS; i++)
    {
      if (linkaddr_cmp(&event_history[i].addr, src))
      {
        event_history[i].time = now;
        exists = true;
        break;
      }
      if (event_history[i].time < event_history[oldest].time)
      {
        oldest = i;
      }
    }

    if (!exists)
    {
      event_history[oldest].time = now;
      linkaddr_copy(&event_history[oldest].addr, src);
    }
  }

  // 2. Count how many unique nodes are active within the 30s window
  int fresh_count = 0;
  for (i = 0; i < MAX_NUMBER_OF_EVENTS; i++)
  {
    if ((now - event_history[i].time < EVENT_TIMEOUT) &&
        (!linkaddr_cmp(&event_history[i].addr, &linkaddr_null)))
    {
      fresh_count++;
    }
  }

  // 3. LED and Console Output Logic
  if (fresh_count >= 3)
  {
    leds_on(LEDS_BLUE);
    // This will show up in the Mote Output window for Node 4
    printf("ALARM: 3 distinct nodes detected! (Count: %d)\n", fresh_count);
  }
  else
  {
    leds_on(LEDS_BLUE); // Safety: if count drops below 3, turn off Blue LED
    leds_off(LEDS_BLUE);
  }
}

/*---------------------------------------------------------------------------*/
static void recv(const void *data, uint16_t len,
                 const linkaddr_t *src, const linkaddr_t *dest)
{
  printf("Received click from node %d\n", src->u8[0]);
  handle_event(src);
  leds_toggle(LEDS_GREEN);
}

/*---------------------------------------------------------------------------*/
PROCESS(clicker_ng_process, "Clicker NG Process");
AUTOSTART_PROCESSES(&clicker_ng_process);

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(clicker_ng_process, ev, data)
{
  static char payload[] = "hej";

  PROCESS_BEGIN();

  /* Initialize NullNet */
  nullnet_buf = (uint8_t *)&payload;
  nullnet_len = sizeof(payload);
  nullnet_set_input_callback(recv);

  /* Activate hardware */
  SENSORS_ACTIVATE(button_sensor);

  /* Set timer to re-check the history every 2 seconds */
  etimer_set(&check_timer, CLOCK_SECOND * 2);

  while (1)
  {
    PROCESS_WAIT_EVENT();

    // Handle Local Button Click
    if (ev == sensors_event && data == &button_sensor)
    {
      printf("I pressed my own button!\n");
      leds_toggle(LEDS_RED);

      // Update our own local record
      handle_event(&linkaddr_node_addr);

      // Tell the neighbors
      memcpy(nullnet_buf, &payload, sizeof(payload));
      nullnet_len = sizeof(payload);
      NETSTACK_NETWORK.output(NULL);
    }

    // Handle Timer (turns off LED if no events happen for 30s)
    if (ev == PROCESS_EVENT_TIMER && data == &check_timer)
    {
      handle_event(&linkaddr_null);
      etimer_reset(&check_timer);
    }
  }

  PROCESS_END();
}