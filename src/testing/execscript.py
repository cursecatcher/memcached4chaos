#! /usr/bin/python

import os
import sys


def execute(nw, nr, ntest):
    command = "./cachetest " + str(nw) + " " + str(nr)
    print "-- Testing " +  command +  "..."
    command += " logfile%d.data"

    for test in xrange(ntest):
        os.system(command % (test + 1))


#parametri:
#argv[1] -> max writers
#argv[2] -> max readers
#argv[3] -> num test per ogni combinazione
if __name__ == "__main__":
    if len(sys.argv) != 4:
        print "Errore nei parametri"
        print "Parametro #1: numero massimo writers"
        print "Parametro #2: numero massimo readers"
        print "Parametro #3: numero test per combinazione"
        print "Aborted."
        sys.exit()

    data = dict()

    try:
        data["maxw"] = int(sys.argv[1])
        data["maxr"] = int(sys.argv[2])
        data["ntest"] = int(sys.argv[3])
    except:
        print "I parametri devono essere valori interi > 0"
        print "Aborted."
        sys.exit()

    print "\nRiepilogo dati di testing:"
    print "Massimo numero di writer: " + sys.argv[1]
    print "Massimo numero di reader: " + sys.argv[2]
    print "Tentativi per test: " + sys.argv[3]

    #situazione 1:1
    print "\n- Test 1:1"
    execute(1, 1, data["ntest"])

    #situazione 1:n
    print "\n- Test 1:n"

    for nreader in xrange(20, data["maxr"] + 1, 20):
        execute(1, nreader, data["ntest"])

    #situazione n:1
    print "\n- Test n:1"

    for nwriter in xrange(20, data["maxw"] + 1, 20):
        execute(nwriter, 1, data["ntest"])

    #situazione n writer - n reader
    print "\n- Test n:n"

    for nwriter in xrange(20, data["maxw"] + 1, 20):
        for nreader in xrange(20, data["maxr"] + 1, 20):
            execute(nwriter, nreader, data["ntest"])

    print "\nTest terminati"
