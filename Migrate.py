import sqlite3
import sys
import requests
import json
import signal
import threading
import time
import os
try:
    import Queue as Queue
except ImportError:
    try:
        import queue as Queue
    except ImportError:
        raise
DB=None
Q=Queue.Queue()
L=threading.Lock()
Flag=True
def H(signal, frame):
    L.acquire()
    DB.commit()
    DB.close()
    os.kill(os.getpid(), 9)
def Handle():
    while Flag:
        item=Q.get()
        uid=int(item[0])
        inf=json.loads(item[7])
        cpid=int(item[2])
        rpid=int(item[3])
        dpid=int(item[4])
        udid=int(item[5])
        postid=int(item[6])
        stmt="UPDATE WorkInfo SET cp_id=0,rp_id=0,dp_id=0,ud_id=0,post_id=0,item_id=? WHERE cp_id=? AND rp_id=? AND dp_id=? AND ud_id=? AND post_id=?"
        params=[0,cpid,rpid,dpid,udid,postid];
        if inf.get("item_id")!=None:
            params[0]=int(inf["item_id"])
            L.acquire()
            DB.execute(stmt,params)
            L.release()
        else:
            try:
                URLBase="https://bcy.net/"
                if cpid!=0 and rpid!=0:
                    params[0]=requests.get("https://bcy.net/coser/detail/"+str(cpid)+"/"+str(rpid),allow_redirects=False, timeout=5).headers["Location"].replace("/item/detail/","")
                    L.acquire()
                    DB.execute(stmt,params)
                    L.release()
                elif udid!=0:
                    params[0]=requests.get("https://bcy.net/daily/detail/"+str(udid),allow_redirects=False, timeout=5).headers["Location"].replace("/item/detail/","")
                    L.acquire()
                    DB.execute(stmt,params)
                    L.release()
                elif postid!=0:
                    gid=inf["gid"]
                    params[0]=requests.get("http://bcy.net/group/detail/"+str(gid)+"/"+str(udid),allow_redirects=False, timeout=5).headers["Location"].replace("/item/detail/","")
                    L.acquire()
                    DB.execute(stmt,params)
                    L.release()
                elif dpid!=0:
                    params[0]=requests.get("https://bcy.net/illust/detail/"+str(dpid),allow_redirects=False, timeout=5).headers["Location"].replace("/item/detail/","")
                    L.acquire()
                    DB.execute(stmt,params)
                    L.release()
                else:
                    print(item)
            except:
                pass

    Q.task_done()
if __name__ == '__main__':
    if len(sys.argv)!=2:
        print("Usage: "+sys.argv[0]+" /PATH/TO/DB")
    DBPath=sys.argv[1]
    for i in range(32):
        t =  threading.Thread(target=Handle)
        t.start()
    DB=sqlite3.connect(DBPath,check_same_thread=False)
    Items=DB.execute("SELECT * FROM WorkInfo WHERE item_id=0").fetchall()
    signal.signal(signal.SIGINT, H)
    print("Found "+str(len(Items))+" Items")
    for item in Items:
        Q.put(item)
    while Q.empty()==False:
        print("Queue Size:"+str(Q.qsize()))
        time.sleep(5)
    Flag=False
    DB.commit()
    Items=DB.execute("SELECT * FROM WorkInfo WHERE item_id=0").fetchall()
    if len(Items)!=0:
        print("Some WorkInfo Failed to Migrate")
    else:
        DB.execute("CREATE TABLE IF NOT EXISTS WorkInfo_NEW (uid INTEGER DEFAULT 0,Title STRING NOT NULL DEFAULT '',item_id INTEGER DEFAULT 0,Info STRING NOT NULL DEFAULT '',Tags STRING NOT NULL DEFAULT '[]',UNIQUE (item_id) ON CONFLICT REPLACE)")
        DB.execute("INSERT INTO WorkInfo_NEW SELECT uid,Title,item_id,Info,Tags FROM WorkInfo;")
        DB.execute("DROP TABLE IF EXISTS WorkInfo;")
        DB.execute("ALTER TABLE WorkInfo_NEW RENAME TO WorkInfo;")
    DB.commit()
    DB.close()
