#ifndef XLINK_CONTROL_H
#define XLINK_CONTROL_H

#include "xlink.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define XLINK_COMP_ID_CONTROL 2

#define XLINK_CONTROL_MSG_ID_LED_CTRL 0
#define XLINK_CONTROL_MSG_ID_LED_STATE 1
#define XLINK_CONTROL_MSG_ID_TEMPTURE_STATE 2

typedef xlink_packed(struct xlink_control_led_ctrl_t_def
{
    bool cmd;
}) xlink_control_led_ctrl_t;

static inline int xlink_control_led_ctrl_send(xlink_context_p context, bool cmd)
{
    xlink_control_led_ctrl_t msg;
    msg.cmd = cmd;
    return xlink_send(context, XLINK_COMP_ID_CONTROL, XLINK_CONTROL_MSG_ID_LED_CTRL, (const uint8_t *)&msg, (uint8_t)sizeof(msg));
}

typedef xlink_packed(struct xlink_control_led_state_t_def
{
    bool state;
}) xlink_control_led_state_t;

static inline int xlink_control_led_state_send(xlink_context_p context, bool state)
{
    xlink_control_led_state_t msg;
    msg.state = state;
    return xlink_send(context, XLINK_COMP_ID_CONTROL, XLINK_CONTROL_MSG_ID_LED_STATE, (const uint8_t *)&msg, (uint8_t)sizeof(msg));
}

typedef xlink_packed(struct xlink_control_tempture_state_t_def
{
    float tempture[5];
}) xlink_control_tempture_state_t;

static inline int xlink_control_tempture_state_send(xlink_context_p context, const float *tempture)
{
    xlink_control_tempture_state_t msg;
    if (tempture == NULL)
    {
        return -1;
    }
    memcpy(msg.tempture, tempture, sizeof(msg.tempture[0]) * 5u);
    return xlink_send(context, XLINK_COMP_ID_CONTROL, XLINK_CONTROL_MSG_ID_TEMPTURE_STATE, (const uint8_t *)&msg, (uint8_t)sizeof(msg));
}

#endif // XLINK_CONTROL_H
