# emonTX_Keyestudio_Ethernet
emonTX arduino shield hack & connected to Keyestudio KS0304 Ethernet board
  
  
  ## 1. emonTX arduino shield SMT
  
  The shield is based on emonTX arduino shield.
  Allo information are available [here](https://github.com/openenergymonitor/emontx-shield)
    
  The shield allows 1x AC Voltage measurement + 4x CT current sensors.
  But optical pulse counting interface is missing, but the board provide a DS18B20 connector interface as well as an access to the arduino IRQ pins.
  As RFM interface is not using, a single wire or a jumper allows to convert the DS18B20 interface to a pulse counting interface.
  
  ![emonTX_modified](http://meteomonclar.free.fr/svg/emon/interrupt.svg)

  





If you don't want to cut the connector of the optical sensor, you can use a RJ45 adapter:

  ![rj45adapter](http://meteomonclar.free.fr/svg/emon/RJ45_adapter.svg)
