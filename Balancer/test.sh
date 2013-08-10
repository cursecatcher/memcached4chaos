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

2)      
	    ./Server/bin/Release/Server 1101 192.168.1.1 &
	    sleep 0.5
	    ./Server/bin/Release/Server 1102 192.168.1.1 &
;;

3)      clear; echo "How many Setters do you want to launch?"
	read SETTERS_NUM
	SETTER_NAME="set";
	BALANCER_ADDR="192.168.1.1";
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
	clear; echo "How many Setters do you want to read?"
	read SETTERS_NUM
	GET_INDEX=-1;
	SETTER_NAME="set"
	BALANCER_ADDR="192.168.1.1";
	COUNTER=0

         while [  $COUNTER -lt $GETTERS_NUM ];
	do
	    SET_INDEX=$(($COUNTER % $SETTERS_NUM));
	    if [ $SET_INDEX = 0 ]; then let GET_INDEX=GET_INDEX+1; fi;
  	    NAME=${SETTER_NAME}${SET_INDEX}"_get_"${GET_INDEX}
	   ./Getter/bin/Release/Getter -b $BALANCER_ADDR -n ${NAME} -s 1024 -ui 1000 -m ${SETTER_NAME}${SET_INDEX} &
	    let COUNTER=COUNTER+1
	    sleep 0.5
	done
;;
'q')    
;;
'Q')
;;
esac