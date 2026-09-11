#include "MqttMessageProtocol.h"

static_assert(MqttCommandKind::kDcMotorForward != MqttCommandKind::kInvalid);
static_assert(MqttCommandKind::kDcMotorReverse != MqttCommandKind::kInvalid);
static_assert(MqttCommandKind::kDcMotorStop != MqttCommandKind::kInvalid);

int main() { return 0; }
