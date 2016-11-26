
Makefile裡的
`-DSINGLE_MODE` = single concurrent process
沒有加的話就是 multiple processes

用mutex在跨process似乎會有問題，所以就全部拿掉讓他race了(笑
