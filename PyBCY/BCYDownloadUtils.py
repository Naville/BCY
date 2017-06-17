# -*- coding: utf-8 -*-
import os,errno,tempfile,sqlite3,sys,shutil,threading,time,json,struct,requests
from PyBCY.BCYDownloadFilter import *
from PyBCY.BCYCore import *
from sys import version as python_version
from cgi import parse_header, parse_multipart

if python_version.startswith('3'):
    from urllib.parse import parse_qs
    from http.server import BaseHTTPRequestHandler, HTTPServer
else:
    from urlparse import parse_qs
    from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
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
        self.ParseWebURL(Info["URL"])
        self.send_response(200, "ok")
    def ParseWebURL(self,URL):
        Regex=r'(?P<Type>illust|coser|daily)\/detail\/(?P<Component>[0-9\/]*)'
        Regex2=r'bcy.net\/u\/([0-9]*)'
        R1=re.compile(Regex)
        R2=re.compile(Regex2)
        m=R1.search(URL)
        m2=R2.search(URL)

        if m2!=None:
            self.server.DownloadUtils.logger.warning("HTTPDownloadRequest User uid:%s"%(m2.group(0)))
            self.server.DownloadUtils.DownloadUser(m2.group(0),"all")
        if m!=None:
            if m.group("Type")=="daily":
                self.server.DownloadUtils.logger.warning("HTTPDownloading Daily %s"%m.group("Component"))
                self.server.DownloadUtils.DownloadFromAbstractInfo({"ud_id":m.group("Component")})
            elif m.group("Type")=="coser":
                cp_id=m.group("Component").split("/")[0]
                rp_id=m.group("Component").split("/")[1]
                self.server.DownloadUtils.logger.warning("HTTPDownloadRequest Cosplay cp_id:%s rp_id:%s"%(cp_id,rp_id))
                self.server.DownloadUtils.DownloadFromAbstractInfo({"cp_id":cp_id,"rp_id":rp_id})
            elif m.group("Type")=="illust":
                dp_id=m.group("Component").split("/")[0]
                rp_id=m.group("Component").split("/")[1]
                self.server.DownloadUtils.logger.warning("HTTPDownloadRequest Illust dp_id:%s rp_id:%s"%(dp_id,rp_id))
                self.server.DownloadUtils.DownloadFromAbstractInfo({"dp_id":dp_id,"rp_id":rp_id})
            else:
                return
class BCYDownloadUtils(object):
    '''
        self.DownloadProcesses          A dictionary. For each entry, key is the URL being downloaded
                                            The object is a tuple of structure(DownloadedSize,TotalSize)
        self.logger                     A logging.logger() object
        self.Filter                     A BCYDownloadFilter() object used for filtering downloads
        self.API                        A BCYCore() object
        self.FailedInfoList             A list of DetailedInfo of failed downloads

        BCYDownloadUtils is underlying powered by two FIFO Queues:
            QueryQueue          For Storing AbstractInfo
            DownloadQueue       Storing Info used by DownloadWorker(). DO NOT INVOKE DIRECTLY

        And all interfaces in BCYDownloadUtils is actually just retreive and push AbstractInfo to the Queue.
        So when you see a log message indicating something is being downloaded, that only means the AbstractInfo
        has been pushed to the queue.
    '''
    def __init__(self,email,password,savepath,MaxDownloadThread=16,MaxQueryThread=64,Daemon=False,IP="127.0.0.1",Port=8081,DownloadProgress=False):
        self.QueryQueue=Queue.Queue()
        self.DownloadQueue=Queue.Queue()
        self.SavePath=savepath
        self.logger=logging.getLogger(str(__name__)+"-"+str(hex(id(self))))
        self.logger.addHandler(logging.NullHandler())
        self.Filter=BCYDownloadFilter()
        self.API=BCYCore()
        self.FailedInfoList=list()
        self.API.loginWithEmailAndPassWord(email,password)
        print ("Logged in...UID:"+str(self.API.UID))
        self.InfoSQL=sqlite3.connect(os.path.join(savepath,"BCYInfo.db"),check_same_thread=False)
        self.InfoSQL.text_factory = str
        self.InfoSQL.execute("CREATE TABLE IF NOT EXISTS UserInfo (uid STRING,UserName STRING);")
        self.InfoSQL.execute("CREATE TABLE IF NOT EXISTS GroupInfo (gid STRING,GroupName STRING);")
        self.InfoSQL.execute("CREATE TABLE IF NOT EXISTS WorkInfo (uid STRING NOT NULL DEFAULT '',Title STRING NOT NULL DEFAULT '',cp_id STRING NOT NULL DEFAULT '',rp_id STRING NOT NULL DEFAULT '',dp_id STRING NOT NULL DEFAULT '',ud_id STRING NOT NULL DEFAULT '',post_id STRING NOT NULL DEFAULT '',Info STRING NOT NULL DEFAULT '',Tags STRING,UNIQUE(UID,cp_id,rp_id,dp_id,ud_id,post_id) ON CONFLICT REPLACE);")
        self.InfoSQLLock=threading.Lock()
        self.InfoSQLLock=threading.Lock()
        self.DownloadProcesses=dict()
        #Create Threading Events
        self.DownloadWorkerEvent=threading.Event()
        self.QueryEvent=threading.Event()
        self.DownloadWorkerEvent.set()
        self.QueryEvent.set()
        #End Threading Events Init

        #Prepare Threads
        for i in range(MaxDownloadThread):
            t =  threading.Thread(target=self.DownloadWorker)
            t.daemon = Daemon
            t.start()
        for i in range(MaxQueryThread):
            t =  threading.Thread(target=self.DownloadFromAbstractInfoWorker)
            t.daemon = Daemon
            t.start()
        #Wakeup
        if Daemon==True:
            server_address = (IP,int(Port))
            httpd = HTTPServer(server_address, ServerHandler)
            httpd.DownloadUtils=self
            t =  threading.Thread(target=httpd.serve_forever)
            t.daemon = Daemon
            t.start()
        #Create Temporary Folder
        try:
            os.makedirs(os.path.join(self.SavePath,"DownloadTemp/"))
        except OSError as exception:
            if exception.errno != errno.EEXIST:
                raise
        self.DownloadProgress=DownloadProgress

    def DownloadWorker(self):
        '''
        DownloadWorker Spawned From __init__
        '''
        def Logger(URL,Downloaded,total_length):
            if Downloaded==total_length:
                self.DownloadProcesses.pop(URL, None)
            self.DownloadProcesses[URL]=(Downloaded,total_length)

        while self.DownloadWorkerEvent.isSet()==True:
            try:
                obj=self.DownloadQueue.get()
                URL=obj[0]
                ID=obj[1]
                WorkType=obj[2]
                SaveBase=obj[3]
                FileName=obj[4]
                SavePath=os.path.join(SaveBase,FileName)
                try:
                    os.makedirs(SaveBase)
                except OSError as exception:
                    if exception.errno != errno.EEXIST:
                        raise
                if self.DownloadProgress==True:
                    ImageData=self.API.imageDownload({"url":URL,"id":ID,"type":WorkType},Callback=Logger)
                else:
                    ImageData=self.API.imageDownload({"url":URL,"id":ID,"type":WorkType})
                #Atomic Writing
                if ImageData!=None:
                    f=tempfile.NamedTemporaryFile(dir=os.path.join(self.SavePath,"DownloadTemp/"),delete=False, suffix="PyBCY-")
                    f.write(ImageData)
                    f.close()
                    shutil.move(f.name,SavePath)
            except Queue.Empty:
                pass
            self.DownloadQueue.task_done()
    def DownloadFromInfo(self,Info):
        '''
        Analyze Info and put it into the download queue
        '''
        UID=None
        try:
            UID=Info["uid"]
        except:
            self.FailedInfoList.append(Info)
            return
        Title=None
        if "title" in Info.keys():
            Title=Info["title"]
        CoserName=self.LoadOrSaveUserName(Info["profile"]["uname"],UID)
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
                Title=GroupName+"-"+WorkID
        Title=self.LoadTitle(Title,Info)
        self.SaveInfo(Title,Info)
        WritePathRoot=os.path.join(self.SavePath,str(CoserName).replace("/","-"),str(Title).replace("/","-"))
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
            SavePath=os.path.join(WritePathRoot,FileName)
            if os.path.isfile(SavePath)==False:
                self.DownloadQueue.put([URL,WorkID,WorkType,WritePathRoot,FileName])

    def DownloadUser(self,UID,Filter,**kwargs):
        self.logger.warning("Downloading UID:"+str(UID)+" Filter:"+str(Filter))
        self.API.userWorkList(UID,Filter,Callback=self.DownloadFromAbstractInfo,**kwargs)
    def DownloadGroup(self,GID,Filter,**kwargs):
        self.logger.warning("Downloading GroupGID:"+str(GID)+" Filter:"+str(Filter))
        self.API.groupPostList(GID,Filter,Callback=self.DownloadFromAbstractInfo,**kwargs)
    def DownloadCircle(self,CircleID,Filter,**kwargs):
        self.logger.warning("Downloading CircleID:"+str(CircleID)+" Filter:"+str(Filter))
        self.API.circleList(CircleID,Filter,Callback=self.DownloadFromAbstractInfo,**kwargs)
    def DownloadTag(self,Tag,Filter,**kwargs):
        self.logger.warning("Downloading Tag:"+str(Tag)+" Filter:"+str(Filter))
        self.API.tagList(Tag,Filter,Callback=self.DownloadFromAbstractInfo,**kwargs)
    def DownloadLikedList(self,Filter,**kwargs):
        self.logger.warning("Downloading likedList Filter:"+str(Filter))
        self.API.likedList(Filter,Callback=self.DownloadFromAbstractInfo,**kwargs)
    def DownloadUserRecommends(self,UID,Filter):
        self.logger.warning("Downloading Recommends From UID:"+str(UID)+" And Filter:"+Filter)
        self.API.userRecommends(UID,Filter,Callback=self.DownloadFromAbstractInfo)
    def DownloadSearch(self,Keyword,Type,**kwargs):
        self.logger.warning("Downloading Search Result From Keyword:"+str(Keyword)+" And Type:"+Type)
        self.API.search(Keyword,Type,Callback=self.DownloadFromAbstractInfo,**kwargs)
    def DownloadFromAbstractInfo(self,AbstractInfo):
        '''
            Put AbstractInfo into QueryQueue.
        '''
        self.QueryQueue.put(AbstractInfo)
    def DownloadFromAbstractInfoWorker(self):
        '''
        AbstractInfoQueryWorker Spawned From __init__
        Obtain AbstractInfo from QueryQueue, Query DetailInfo And Put Into Download Queue
        '''
        while self.QueryEvent.isSet()==True:
            try:
                AbstractInfo=self.QueryQueue.get()
                Inf=self.API.queryDetail(AbstractInfo)
                if Inf==None:
                    self.FailedInfoList.append(AbstractInfo)
                else:
                    self.DownloadFromInfo(Inf)
            except Queue.Empty:
                pass
            except AttributeError as e:
                self.FailedInfoList.append(AbstractInfo)
            self.QueryQueue.task_done()
    def LoadOrSaveUserName(self,UserName,UID):
        '''
            Can be used as pure query function when UserName is None
            Return None if UserName is None and corresponding record doesn't exist.
        '''
        self.InfoSQLLock.acquire()
        Q="SELECT UserName FROM UserInfo WHERE uid="+str(UID)
        Cursor=self.InfoSQL.execute(Q)
        for item in Cursor:
            self.InfoSQLLock.release()
            return item[0]
        if UserName==None:
            self.InfoSQLLock.release()
            return None
        self.InfoSQL.execute("INSERT INTO UserInfo (uid, UserName) VALUES (?,?)",(str(UID),UserName,))
        self.InfoSQLLock.release()
        return UserName
    def LoadOrSaveGroupName(self,GroupName,GID):
        '''
            Can be used as pure query function when GroupName is None.
            Return None if GroupName is None and corresponding record doesn't exist.
        '''
        self.InfoSQLLock.acquire()
        Q="SELECT GroupName FROM GroupInfo WHERE gid="+str(GID)
        Cursor=self.InfoSQL.execute(Q)
        for item in Cursor:
            self.InfoSQLLock.release()
            return item[0]
        if GroupName==None:
            return None
        self.InfoSQL.execute("INSERT INTO GroupInfo (gid, GroupName) VALUES (?,?)",(str(GID),GroupName,))
        self.InfoSQLLock.release()
        return GroupName
    def LoadTitle(self,Title,Info):
        '''
            Load from database.Return Title if not exists
        '''
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
        Cursor=self.InfoSQL.execute(Q,tuple(Values))
        for item in Cursor:
            self.InfoSQLLock.release()
            return item[0]
        #We Don't Save Title as it's handled by SaveInfo
        self.InfoSQLLock.release()
        return Title
    def SaveInfo(self,Title,Info):
        self.InfoSQLLock.acquire()
        ValidIDs=dict()
        #Prepare Insert Statement
        ValidIDs["Title"]=Title
        ValidIDs["uid"]=Info["uid"]
        TagList=list()
        for item in Info.get("post_tags",list()):
            TagList.append(item["tag_name"])
        try:
            ValidIDs["Info"]=json.dumps(Info,separators=(',', ':'),ensure_ascii=False, encoding='utf8')
            ValidIDs["Tags"]=json.dumps(TagList,separators=(',', ':'),ensure_ascii=False, encoding='utf8')
        except TypeError:
            ValidIDs["Info"]=json.dumps(Info,separators=(',', ':'),ensure_ascii=False)
            ValidIDs["Tags"]=json.dumps(TagList,separators=(',', ':'),ensure_ascii=False)
        except:
            raise
        InsertQuery="INSERT OR REPLACE INTO WorkInfo ("
        keys=list()
        ValuesPlaceHolders=list()
        Values=list()
        for item in ValidIDs.keys():
            keys.append(item)
            Values.append(ValidIDs[item])
            ValuesPlaceHolders.append("?")
        InsertQuery=InsertQuery+",".join(keys)+")VALUES ("+",".join(ValuesPlaceHolders)+");"
        self.InfoSQL.execute(InsertQuery,tuple(Values))
        self.InfoSQLLock.release()
    def cleanup(self):
        #No need to obtain SQL Mutex as it's controlled by LoadOrSaveUserName()
        FolderList=list()
        for UID in self.Filter.UIDList:
            UserName=self.LoadOrSaveUserName(None,UID)
            if UserName!=None and len(UserName)>0:
                FolderPath=os.path.join(self.SavePath,UserName)
                if (os.path.normpath(FolderPath) != os.path.normpath(self.SavePath)):
                    FolderList.append(FolderPath)
        for Path in FolderList:
            if os.path.isdir(Path):
                self.logger.warning("Removing Folder"+Path)
                shutil.rmtree(Path)
        shutil.rmtree(os.path.join(self.SavePath,"DownloadTemp"))
    def cancel(self):
        '''
        Call this to cancel operations.
        '''
        #Obtain mutex
        self.logger.warning("Clearing Thread Flags")
        self.DownloadWorkerEvent.clear()
        self.QueryEvent.clear()
        self.logger.warning("Obtaining SQL Mutex")
        self.InfoSQLLock.acquire()
        self.logger.warning("Commiting SQLs")
        self.InfoSQL.commit()
        self.logger.warning("Closing SQLs")
        self.InfoSQL.close()
        self.logger.warning("Releasing Lock SQLs")
        self.InfoSQLLock.release()
        return
    def join(self):
        '''
        Blocks until all operations are finished
        '''
        while self.QueryQueue.empty()==False or self.DownloadQueue.empty()==False:
            self.logger.warning ("QueryQueue Size:"+str(self.QueryQueue.qsize()))
            self.logger.warning ("DownloadQueue Size:"+str(self.DownloadQueue.qsize()))
            time.sleep(3)
    def verify(self):
        '''
        Iterate all records in the specified SQL Table and download missing images
        '''
        i=0
        Cursor=self.InfoSQL.execute("SELECT Info FROM WorkInfo").fetchall()
        for item in Cursor:
            print ("Injecting "+str(i)+"/"+str(len(Cursor))+" Into Download Queue")
            Info=json.loads(item[0])
            i=i+1
            self.DownloadFromInfo(Info)
