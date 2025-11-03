/**
 * @file bmb_test.h
 * @brief Unified BMB Test Interface
 * 
 * Provides a unified test interface that works with both Batman SPI 
 * and isoSPI PIO interfaces. Can run tests once or continuously.
 */

#ifndef BMB_TEST_H
#define BMB_TEST_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run BMB test once using active interface
 * 
 * Automatically uses Batman or isoSPI depending on which is active.
 */
void bmb_test_run_once(void);

/**
 * @brief Enable continuous test mode
 * 
 * Tests will run automatically every 2 seconds.
 * 
 * @param enabled true to enable continuous testing, false to disable
 */
void bmb_test_set_continuous(bool enabled);

/**
 * @brief Check if continuous test mode is enabled
 * 
 * @return true if continuous testing is enabled
 */
bool bmb_test_is_continuous(void);

/**
 * @brief Loop function for continuous testing
 * 
 * Call this from main loop. It will automatically run tests
 * every 2 seconds if continuous mode is enabled.
 */
void bmb_test_loop(void);

#ifdef __cplusplus
}
#endif

#endif // BMB_TEST_H

