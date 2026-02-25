#include "contiki.h"
#include "sys/node-id.h"
#include "sys/log.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/ipv6/uip-ds6-nbr.h"
#include "net/ipv6/uip-sr.h"
#include "net/mac/tsch/tsch.h"
#include "net/routing/routing.h"
#include "dev/leds.h"

/* Fix for Sky Mote Blue LED */
#define LEDS_BLUE_FIX LEDS_YELLOW

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_DBG

/*---------------------------------------------------------------------------*/
PROCESS(node_process, "RPL Node");
AUTOSTART_PROCESSES(&node_process);

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  static struct etimer et;

  PROCESS_BEGIN();

  /* Node 1 is the Root */
  if (node_id == 1)
  {
    NETSTACK_ROUTING.root_start();
  }

  NETSTACK_MAC.on();

  /* Update loop every 10 seconds */
  etimer_set(&et, CLOCK_SECOND * 10);

  while (1)
  {
    PROCESS_YIELD_UNTIL(etimer_expired(&et));

    /* 1. Gather status info */
    int num_routes = (int)uip_ds6_route_num_routes();
    bool is_root = (bool)NETSTACK_ROUTING.node_is_root();

    /* Check if we have a default router (meaning we joined the DAG) */
    bool in_network = (uip_ds6_defrt_choose() != NULL);

    /* 2. Reset LEDs */
    leds_off(LEDS_ALL);

    /* 3. Logic based on your Task 2 requirements */
    if (is_root)
    {
      /* Root: GREEN */
      leds_on(LEDS_GREEN);
      LOG_INFO("Node Status: ROOT\n");
    }
    else if (in_network && num_routes > 0)
    {
      /* Intermediate: BLUE (Sky Mote Yellow) */
      leds_on(LEDS_BLUE_FIX);
      LOG_INFO("Node Status: INTERMEDIATE (%d routes)\n", num_routes);
    }
    else if (in_network && num_routes == 0)
    {
      /* Endpoint: RED */
      leds_on(LEDS_RED);
      LOG_INFO("Node Status: ENDPOINT\n");
    }
    else
    {
      /* Searching: OFF */
      LOG_INFO("Node Status: DISCONNECTED/SEARCHING\n");
    }

    etimer_reset(&et);
  }

  PROCESS_END();
}