# EtherCAT adapter contract

The portable repository exposes EtherCAT through the platform API contract.
The board integration selects and builds an appropriate SOEM release and owns
the raw Ethernet interface. Applications must not open that interface directly.
