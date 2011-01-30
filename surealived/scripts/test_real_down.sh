#!/bin/bash

date >> /tmp/notify_real.log
echo "DOWN: [$@]" >> /tmp/notify_real.log
echo "---" >> /tmp/notify_real.log
