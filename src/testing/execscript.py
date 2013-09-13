#! /usr/bin/python

import os
import sys


def execute(nw, nr, ntest, maxthr):

    if nw > 0 and nr > 0 and nw + nr < maxthr:
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
    delta = 0

    if len(sys.argv) != 6:
        print "Errore nei parametri"
        print " #1: numero massimo writers"
        print " #2: numero massimo readers"
        print " #3: numero test per combinazione"
        print " #4: numero massimo thread" #da determinare sperimentalmente
        print " #5: incremento dei w/r tra un test e il successivo"
        print "Aborted."
        sys.exit()

    data = dict()

    try:
        data["maxw"] = int(sys.argv[1])
        data["maxr"] = int(sys.argv[2])
        data["ntest"] = int(sys.argv[3])
        data["nthread"] = int(sys.argv[4])
        delta = int(sys.argv[5])
    except:
        print "I parametri devono essere valori interi > 0"
        print "Aborted."
        sys.exit()

    #situazione 1:1
    print "\n- Test 1:1"
    execute(1, 1, data["ntest"], data["nthread"])

    #situazione 1:n
    print "\n- Test 1:n"

    for nreader in xrange(0, data["maxr"] + 1, delta):
        execute(1, nreader, data["ntest"], data["nthread"])

    #situazione n:1
    print "\n- Test n:1"

    for nwriter in xrange(0, data["maxw"] + 1, delta):
        if nwriter > 1:
            execute(nwriter, 1, data["ntest"], data["nthread"])

    #situazione n writer - n reader
    #~ print "\n- Test n:n"
#~
    #~ for nwriter in xrange(0, data["maxw"] + 1, delta):
        #~ if nwriter > 1:
            #~ for nreader in xrange(0, data["maxr"] + 1, delta):
                #~ if nreader > 1:
                    #~ execute(nwriter, nreader, data["ntest"], data["nthread"])

    print "\nTest terminati\n"

    print "\nRiepilogo dati di testing:"
    print "Massimo numero di writer: " + sys.argv[1]
    print "Massimo numero di reader: " + sys.argv[2]
    print "Tentativi per test: " + sys.argv[3]
    print "Incremento: " + sys.argv[5]
