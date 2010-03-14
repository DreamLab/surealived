#!/bin/bash

date >> /tmp/notify.log
echo "DOWN: [$@]" >> /tmp/notify.log
echo "---" >> /tmp/notify.log
