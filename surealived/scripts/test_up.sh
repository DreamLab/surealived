#!/bin/bash

date >> /tmp/notify.log
echo "UP:   [$@]" >> /tmp/notify.log
echo "---" >> /tmp/notify.log
