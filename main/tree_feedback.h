// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "app_hello.h"
#include "esp_err.h"
/** Wire interaction effects to sound and motor. The UI only reports effect
 * ids; this task owns the BSP motor and queues effects into the music mix. */
esp_err_t tree_feedback_start(app_hello_handle_t controller);
