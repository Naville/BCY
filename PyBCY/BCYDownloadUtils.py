# -*- coding: utf-8 -*-
import os,errno,tempfile,sqlite3,sys,shutil,threading
from PyBCY.BCYDownloadFilter import *
from PyBCY.BCYCore import *
import json,sys,struct,os
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
from sys import version as python_version
from cgi import parse_header, parse_multipart

if python_version.startswith('3'):
    from urllib.parse import parse_qs
    from http.server import BaseHTTPRequestHandler
else:
    from urlparse import parse_qs
    from BaseHTTPServer import BaseHTTPRequestHandler
try:
    reload(sys)
    sys.setdefaultencoding('utf8')
except:
    pass
try:
    import Queue as Queue
except ImportError:
    try:
        import queue as Queue
    except ImportError:
        raise


class ServerHandler(BaseHTTPRequestHandler):
    def do_OPTIONS(self):
        self.send_response(200, "ok")
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header("Access-Control-Allow-Headers", "X-Requested-With")
        self.send_response(200, "ok")
    def do_HEAD(self):
        self._set_headers()
    def do_POST(self):
        Info= self.rfile.read(int(self.headers.getheader('Content-Length')))
        Info=json.loads(Info)
        print ("PyBCY Received HTTP Download Request:"+str(Info["URL"]))
        ABS=self.server.DownloadUtils.API.ParseWebURL(Info["URL"])
        self.send_response(200, "ok")
        self.server.DownloadUtils.DownloadFromAbstractInfo(ABS)
class BCYDownloadUtils(object):
    def __init__(self,email,password,savepath,MaxDownloadThread=16,MaxQueryThread=64,DownloadLoggerRefreshInterval=1,DisplayDownloadProcess=False,Daemon=False,IP="127.0.0.1",Port=8081):
        try:
            os.makedirs(savepath)
        except OSError as exception:
            if exception.errno != errno.EEXIST:
                raise
        self.QueryQueue=Queue.Queue()
        self.DownloadQueue=Queue.Queue()
        self.DisplayDownloadProcess=DisplayDownloadProcess
        for i in range(MaxDownloadThread):
            t =  threading.Thread(target=self.DownloadWorker)
            t.daemon = Daemon
            t.start()
        for i in range(MaxQueryThread):
            t =  threading.Thread(target=self.DownloadFromAbstractInfoWorker)
            t.daemon = Daemon
            t.start()
        self.SavePath=savepath
        self.logger=logging.getLogger(str(__name__)+"-"+str(hex(id(self))))
        self.logger.addHandler(logging.NullHandler())
        self.Filter=BCYDownloadFilter()
        self.API=BCYCore()
        self.FailedInfoList=list()
        self.API.loginWithEmailAndPassWord(email,password)
        print ("Logged in...UID:"+str(self.API.UID))
        self.IDNameSQL=sqlite3.connect(os.path.join(savepath,"BCYUserNameUID.db"),check_same_thread=False)
        self.IDNameSQL.text_factory = str
        self.InfoSQL=sqlite3.connect(os.path.join(savepath,"BCYInfo.db"),check_same_thread=False)
        self.InfoSQL.text_factory = str
        self.IDNameSQL.execute("CREATE TABLE IF NOT EXISTS UserInfo (UID STRING,UserName STRING);")
        self.IDNameSQL.execute("CREATE TABLE IF NOT EXISTS GroupInfo (GID STRING,GroupName STRING);")
        self.InfoSQL.execute("CREATE TABLE IF NOT EXISTS WorkInfo (UID STRING,Title STRING,cp_id STRING,rp_id STRING,dp_id STRING,ud_id STRING,post_id STRING,Info STRING);")
        self.IDNameSQLLock=threading.Lock()
        self.InfoSQLLock=threading.Lock()
        self.DownloadProcesses=dict()
        self.DownloadLoggerRefreshInterval=DownloadLoggerRefreshInterval
        if DisplayDownloadProcess==True:
            t =  threading.Thread(target=self.UpdateDownloadLogger)
            t.daemon = False
            t.start()
        if Daemon==True:
            server_address = (IP,int(Port))
            httpd = HTTPServer(server_address, ServerHandler)
            httpd.DownloadUtils=self
            t =  threading.Thread(target=httpd.serve_forever)
            t.daemon = Daemon
            t.start()

    def UpdateDownloadLogger(self,width=20,fill="*",empty=" "):
        while True:
            time.sleep(self.DownloadLoggerRefreshInterval)
            for URL in sorted(self.DownloadProcesses.keys()):
                dl=self.DownloadProcesses[URL][0]
                total=self.DownloadProcesses[URL][1]
                DownloadedBlocksCount=int(dl*width/float(total))
                RemainingBlocksCount=width-DownloadedBlocksCount
                bar="[%s%s]" % (fill*DownloadedBlocksCount,empty*RemainingBlocksCount)
                self.logger.warning("Downloading "+URL+os.linesep+bar)
            for item in self.logger.handlers:
                item.flush()
    def DownloadWorker(self):
        def Logger(URL,Downloaded,total_length):
            if Downloaded==total_length:
                self.DownloadProcesses.pop(URL, None)
            self.DownloadProcesses[URL]=(Downloaded,total_length)

        while True:
            obj=self.DownloadQueue.get(block=True)
            URL=obj[0]
            ID=obj[1]
            WorkType=obj[2]
            SaveBase=obj[3]
            FileName=obj[4]
            SavePath=os.path.join(SaveBase,FileName)
            if os.path.isfile(SavePath)==False:
                try:
                    os.makedirs(SaveBase)
                except OSError as exception:
                    if exception.errno != errno.EEXIST:
                        raise
                ImageData=None
                if self.DisplayDownloadProcess==True:
                    ImageData=self.API.imageDownload({"url":URL,"id":ID,"type":WorkType},Callback=Logger)
                else:
                    ImageData=self.API.imageDownload({"url":URL,"id":ID,"type":WorkType})
                #Atomic Writing
                if ImageData!=None:
                    f=tempfile.NamedTemporaryFile(delete=False, suffix="PyBCY-")
                    f.write(ImageData)
                    f.close()
                    shutil.move(f.name,SavePath)
            else:
                self.logger.info("%s Already Downloaded" % (URL))
            self.DownloadQueue.task_done()
    def DownloadFromInfo(self,Info):
        UID=None
        try:
            UID=Info["uid"]
        except:
            self.FailedInfoList.append(Info)
            return
        Title=None
        if "title" in Info.keys():
            Title=Info["title"]
        CoserName=self.LoadOrSaveCoserName(Info["profile"]["uname"],UID)
        WorkID=None
        WorkType=None
        if self.Filter.ShouldBlockInfo(Info)==True:
            return
        if "ud_id" in Info.keys():
            WorkID=Info["ud_id"]
            WorkType="daily"
            if(Title==None or len(Title)==0):
                Title="日常-"+str(WorkID)
        elif "cp_id" in Info.keys():
            WorkID=Info["rp_id"]
            WorkType="cos"
            if(Title==None or len(Title)==0):
                Title="Cosplay-"+str(WorkID)
        elif "dp_id" in Info.keys():
            WorkID=Info["rp_id"]
            WorkType="drawer"
            if(Title==None or len(Title)==0):
                Title="绘画-"+str(WorkID)
        elif "post_id" in Info.keys() and "group" in Info.keys():
            WorkID=Info["post_id"]
            WorkType="group"
            GID=Info["group"]["gid"]
            UID=Info["uid"]
            GroupName=self.LoadOrSaveGroupName(Info["group"]["name"],GID)
            if(Title==None or len(Title)==0):
                Title=GroupName+"-"+str(GID)
        Title=self.LoadTitle(Title,Info)
        self.SaveInfo(Title,Info)
        WritePathRoot=os.path.join(self.SavePath,CoserName.replace("/","-"),Title.replace("/","-"))
        for PathDict in Info["multi"]:
            URL=PathDict["path"]
            if len(URL)==0:
                continue
            FileName=os.path.split(URL)[0].split("/")[-1]
            ShouldAppendSuffix=True
            for suffix in [".jpg",".png",".jpeg",".gif"]:
                if FileName.lower().endswith(suffix):
                    ShouldAppendSuffix=False
                    break
            if ShouldAppendSuffix==True:
                FileName=FileName+".jpg"
            self.DownloadQueue.put([URL,WorkID,WorkType,WritePathRoot,FileName])

    def DownloadCoser(self,CoserUID,Filter):
        self.API.userWorkList(CoserUID,Filter,Callback=self.DownloadFromAbstractInfo)
    def DownloadGroup(self,GID,Filter):
        self.API.groupPostList(GID,Filter,Callback=self.DownloadFromAbstractInfo)
    def DownloadCircle(self,CircleID,Filter):
        self.API.circleList(CircleID,Filter,Callback=self.DownloadFromAbstractInfo)
    def DownloadTag(self,Tag,Filter):
        self.API.tagList(Tag,Filter,Callback=self.DownloadFromAbstractInfo)
    def DownloadLikedList(self,Filter):
        self.API.likedList(Filter,Callback=self.DownloadFromAbstractInfo)
    def DownloadFromAbstractInfo(self,AbstractInfo):
        self.QueryQueue.put(AbstractInfo)
    def DownloadFromAbstractInfoWorker(self):
        while True:
            try:
                AbstractInfo=self.QueryQueue.get(block=True)
                Inf=self.API.queryDetail(AbstractInfo)
                if Inf==None:
                    self.FailedInfoList.append(AbstractInfo)
                else:
                    self.DownloadFromInfo(Inf)
            except AttributeError as e:
                self.FailedInfoList.append(AbstractInfo)
            self.QueryQueue.task_done()
    def LoadOrSaveCoserName(self,UserName,UID):
        self.IDNameSQLLock.acquire()
        Q="SELECT * FROM UserInfo WHERE UID="+str(UID)
        Cursor=self.IDNameSQL.execute(Q)
        for item in Cursor:
            self.IDNameSQLLock.release()
            return item[1]
        self.IDNameSQL.execute("INSERT INTO UserInfo (UID, UserName) VALUES (?,?)",(str(UID),UserName))
        self.IDNameSQLLock.release()
        return UserName
    def LoadOrSaveGroupName(self,GroupName,GID):
        self.IDNameSQLLock.acquire()
        Q="SELECT * FROM GroupInfo WHERE GID="+str(GID)
        Cursor=self.IDNameSQL.execute(Q)
        for item in Cursor:
            self.IDNameSQLLock.release()
            return item[1]
        self.IDNameSQL.execute("INSERT INTO GroupInfo (GID, GroupName) VALUES (?,?)",(str(GID),GroupName))
        self.IDNameSQLLock.release()
        return GroupName
    def LoadTitle(self,Title,Info):
        self.InfoSQLLock.acquire()
        ValidIDs=dict()
        for key in ["cp_id","rp_id","dp_id","ud_id","post_id"]:
            value=Info.get(key)
            if value!=None:
                ValidIDs[key]=value
        Q="SELECT Title FROM WorkInfo WHERE "
        ArgsList=list()

        #Construct A List of constraints
        keys=list()
        Values=list()

        #Construct A List of constraints
        for item in ValidIDs.keys():
            keys.append(item+"=?")
            Values.append(ValidIDs[item])
        Q=Q+" AND ".join(keys)
        Cursor=self.InfoSQL.execute(Q,Values)
        for item in Cursor:
            self.InfoSQLLock.release()
            return item[0]
        self.InfoSQLLock.release()
        return Title
    def SaveInfo(self,Title,Info):
        self.InfoSQLLock.acquire()
        ValidIDs=dict()
        for key in ["cp_id","rp_id","dp_id","ud_id","post_id"]:
            value=Info.get(key)
            if value!=None:
                ValidIDs[key]=value
        DeleteQuery="DELETE FROM WorkInfo WHERE "
        keys=list()
        Values=list()

        #Construct A List of constraints
        for item in ValidIDs.keys():
            keys.append(item+"=?")
            Values.append(ValidIDs[item])
        DeleteQuery=DeleteQuery+" AND ".join(keys)
        self.InfoSQL.execute(DeleteQuery,Values)
        #Prepare Insert Statement
        ValidIDs["Title"]=Title
        ValidIDs["UID"]=Info["uid"]
        ValidIDs["Info"]=base64.b64encode(json.dumps(Info,separators=(',', ':'),ensure_ascii=False, encoding='utf8'))
        InsertQuery="INSERT INTO WorkInfo ("
        keys=list()
        ValuesPlaceHolders=list()
        Values=list()
        for item in ValidIDs.keys():
            keys.append(item)
            Values.append(ValidIDs[item])
            ValuesPlaceHolders.append("?")
        InsertQuery=InsertQuery+",".join(keys)+")VALUES ("+",".join(ValuesPlaceHolders)+");"
        self.InfoSQL.execute(InsertQuery,Values)
        self.InfoSQLLock.release()
