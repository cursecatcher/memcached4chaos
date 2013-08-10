clear
echo "
***********************
      BALANCER
***********************

Enter an Option:
1 - Launch Balancer
2 - Launch Server(s)
3 - Launch Setter(s)
4 - Launch Getter(s)

q - Quit
";

read OPT;

case "$OPT" in
1)	clear
	./Balancer/bin/Release/Balancer
;;

2)      clear; echo "How many servers do you want to launch?"
	read SERVERS_NUM
	PORT_BASE=1100
	clear; echo "Enter server port index (starting port = 1100+INDEX)"
	read PORT_INDEX
	echo "port = $PORT_INDEX"
	clear; echo "Enter Balancer IP address (default:localhost)"
	read BALANCER_ADDR
	if [ "$BALANCER_ADDR" == "" ]; then BALANCER_ADDR="127.0.0.1"; fi
	COUNTER=0
        while [  $COUNTER -lt $SERVERS_NUM ];
	  do
	    let PORT=$(($PORT_BASE + $PORT_INDEX + $COUNTER));
	    let COUNTER=COUNTER+1
	    #./Server/bin/Release/Server $PORT $BALANCER_ADDR 2> "logs/"$PORT"_errlog" &
	    ./Server/bin/Release/Server $PORT $BALANCER_ADDR &
	    sleep 0.5
	  done
;;

3)      clear; echo "How many Setters do you want to launch?"
	read SETTERS_NUM
	clear; echo "Enter Setter(s) base name (default:'set')"
	read SETTER_NAME
	if [ "$SETTER_NAME" == "" ]; then SETTER_NAME="set"; fi
	clear; echo "Enter Balancer IP address (default:localhost)"
	read BALANCER_ADDR
	if [ "$BALANCER_ADDR" == "" ]; then BALANCER_ADDR="127.0.0.1"; fi
	COUNTER=0
         while [  $COUNTER -lt $SETTERS_NUM ];
	do
	   ./Setter/bin/Release/Setter -b $BALANCER_ADDR -n ${SETTER_NAME}${COUNTER} -s 1024  -ui 1000 &
	    let COUNTER=COUNTER+1
	    sleep 0.5
	done
;;
4)      clear; echo "How many Getters do you want to launch?"
	read GETTERS_NUM
	clear; echo "Enter the name of the SETTER to monitor(default:'set0')"
	read SETTER_NAME
	if [ "$SETTER_NAME" == "" ]; then SETTER_NAME="set0"; fi
	clear; echo "Enter Balancer service IP address (default:localhost)"
	read BALANCER_ADDR
	if [ "$BALANCER_ADDR" == "" ]; then BALANCER_ADDR="127.0.0.1"; fi
	COUNTER=0
	NAME=${SETTER_NAME}"_get_"
         while [  $COUNTER -lt $GETTERS_NUM ];
	do
	   ./Getter/bin/Release/Getter -b $BALANCER_ADDR -n ${NAME}${COUNTER} -s 1024 -ui 1000 -m ${SETTER_NAME} &
	    let COUNTER=COUNTER+1
	    sleep 0.5
	done
;;
'q')    
;;
'Q')
;;
esac
