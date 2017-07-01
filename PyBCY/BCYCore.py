# -*- coding: utf-8 -*-
import requests,base64,logging,json,os,time,tempfile,sys,logging,re
from Crypto.Cipher import AES
from pkcs7 import PKCS7Encoder
from copy import deepcopy
try:
    reload(sys)
    sys.setdefaultencoding('utf8')
except:
    pass
class BCYCore(object):
    '''
    Possible Filter Values:
         - coser
         - all
         - drawer
         - writer
         - daily
         - zhipin
    Possible WorkType Values:
         - illust
         - coser
         - daily
         - group
         - circle
         - group

    Each work,regardless of WorkType, is globally identified using a set of Keys & Values
    Different WorkType of work has different set of used keys.Refer to queryDetail()'s code
    Most APIs use WorkType in URL to identifier WorkType and respond accordingly
    '''
    APIBase="http://api.bcy.net/api/"
    Header={"User-Agent":"bcy/3.8.0 (iPad; iOS 9.3.3; Scale/2.00)","X-BCY-Version":"iOS-3.8.0"}
    NeedFollowServerResponse="阿拉，请先关注一下嘛"
    LockedServerResponse="该作品已被管理员锁定"
    APIErrorResponse="半次元不小心滑了一跤"
    GroupLockedResponse="该讨论已被管理员锁定"
    StatusMap={100:"请先登录半次元",
               4000:"请先登录半次元",
               21:"半次元不小心滑了一跤"
                }
    def __init__(self,Timeout=20):
        self.UID=None
        self.Token=None
        # Lack Support of PKCS7Padding.Solve it in actual crypto stage
        self.Crypto=AES.new('com_banciyuan_AI', AES.MODE_ECB)
        self.pkcs7=PKCS7Encoder()
        self.Timeout=Timeout
        self.session=requests.Session()
        self.session.headers.update(BCYCore.Header)
    def loginWithEmailAndPassWord(self,email,password):
        '''
        Login is not required.
        However certain operations like viewing Follower-Only content might fail in anonymous mode
        '''
        data=self.POST("user/login",{"pass":password,"user":email},timeout=self.Timeout).content
        data=json.loads(data)["data"]
        try:
            self.UID=data["uid"]
            self.Token = data["token"]
        except:
            raise ValueError("BCYLogin Error:"+str(data))
    def EncryptData(self,Data):
        Data=Data.encode("utf8")
        Data=self.pkcs7.encode(Data)
        return self.Crypto.encrypt(Data)
    def EncryptParam(self,Params):
        ParamData=None
        try:
            ParamData=json.dumps(Params,separators=(',', ':'),ensure_ascii=False, encoding='utf8')
        except TypeError:
            ParamData=json.dumps(Params,separators=(',', ':'),ensure_ascii=False)
        return {"data":base64.b64encode(self.EncryptData(ParamData))}
    def GET(self,URL,Params,**kwargs):
        time.sleep(1)
        try:
            return self.session.get(URL,params=Params,**kwargs)
        except:
            return None

    def POST(self,URL,Params,Auth=True,**kwargs):
        time.sleep(1)
        if URL.startswith(BCYCore.APIBase)==False:
                URL=BCYCore.APIBase + URL
        if Auth==True:
            if 'uid' not in Params.keys() and self.UID!=None:
                Params['uid']=self.UID
            if 'token' not in Params.keys() and self.Token!=None:
                Params['token']=self.Token
        P=self.EncryptParam(Params)
        try:
            return self.session.post(URL,data=P,**kwargs)
        except:
            return None
    def userDetail(self,UID):
        try:
            return json.loads(self.POST("user/detail",{"face":"b","uid":UID},timeout=self.Timeout).content)
        except:
            return None
    def hot(self):
        try:
            json.loads(self.POST("coser/listallcore",{"face":"b"},timeout=self.Timeout).content)
        except:
            return None
    def getReply(self,WorkType,Params,**kwargs):
        '''
        Params is WorkIdentifiers
        '''
        URL=WorkType+"/getReply"
        return self.ListIterator(URL,Params,**kwargs)
    def downloadWorker(self,URL,Callback=None):
        Content=None
        req=self.GET(URL,{},timeout=self.Timeout,stream=True)
        total_length = int(req.headers.get('Content-Length'))
        if req==None:
            return None
        if Callback==None:
            Data=req.content
            if(len(Data)==total_length):
                return Data
            else:
                return None
        Downloaded = 0
        for data in req.iter_content(chunk_size=204800):
            Downloaded += len(data)
            if Content==None:
                Content=data
            else:
                Content+=data
            Callback(URL,Downloaded,total_length)
        if Downloaded==total_length:
            Callback(URL,Downloaded,total_length)
            return Content
        else:
            return None

    def imageDownload(self,ImageInfo,Callback=None):
        ImageData=None
        URL=ImageInfo["url"]
        if URL.startswith("http")==False:
            return None
        OriginalImageURL=os.path.split(URL)[0]
        try:
            ImageData=self.downloadWorker(OriginalImageURL,Callback=Callback)
        except:
            pass
        if ImageData==None:#API Endpoint Bug Sometimes Results In Non-Existing URL In Detail Response
            return None
        try:
            rep=json.loads(ImageData)
        except ValueError:
            return ImageData
        ImageData=self.POST("image/download",ImageInfo,timeout=self.Timeout)
        if ImageData==None:#API Endpoint Bug Sometimes Results In Non-Existing URL In Detail Response
            return None
        ImageData=ImageData.content
        try:
            rep=json.loads(ImageData)
            ActualURL=rep["data"]["cover"]
            return self.downloadWorker(ActualURL,Callback=Callback)
        except:
            return None
        return None
    def ListIterator(self,URL,Params,SinceRetrieveKeyName="ctime",SinceSetKeyName="since",Callback=None,since=int(time.time()),to=0,BodyBlock=None):
        '''
        URL and Params are the regular stuff for POST()
        SinceRetrieveKeyName AND SinceSetKeyName are for getting/setting timestamps.
        Callback is called each time a new item appears from the API response
        BodyBlock is for performing custom iterating context, as some of the APIs store
        content in ["data"], others not.
        BodyBlock is called with(FetchedItemList,"data" value of current json response,CallbackFunction)
            Return True in BodyBlock To Stop Iterating. Refer to search() for usage example

        This over-complicate method only exists because the FUCKING STUPID BCY API Design

        '''

        p=1
        items=list()
        while True:
            pa=deepcopy(Params)
            pa["limit"]=20
            pa["p"]=p
            if since!=None and SinceSetKeyName!=None:
                pa[SinceSetKeyName]=since
            p=p+1
            try:
                data=self.POST(URL,pa,timeout=self.Timeout).content
                inf=json.loads(data)["data"]
                if BodyBlock==None:
                    if len(inf)==0:
                        return items
                    for item in inf:
                        if SinceRetrieveKeyName!=None:
                            if int(item[SinceRetrieveKeyName])<to:
                                return items
                            if int(item[SinceRetrieveKeyName])<since:
                                since=int(item[SinceRetrieveKeyName])
                        if Callback!=None:
                            Callback(item)
                    items.extend(inf)
                else:
                    if BodyBlock(items,inf,Callback)==True:
                        return items
            except:
                return items
        return items
    def followUser(self,uid):
        try:
            data=json.loads(self.POST("user/follow",{"uid":uid,"type":"dofollow"},timeout=self.Timeout).content)
            return int(data["status"])==1
        except:
            return False
    def unfollowUser(self,uid):
        try:
            data=json.loads(self.POST("user/follow",{"uid":uid,"type":"unfollow"},timeout=self.Timeout).content)
            return int(data["status"])==1
        except:
            return False
    def search(self,keyword,type,**kwargs):
        '''
        type is one of:
                Cos
                Tags
                Works
                Daily
                Illust
                Goods
                Novel
                Post
                Content
                User
                Group
                All
        '''
        def foo(items,Info,Callback):
            inf=Info["results"]
            if len(inf)==0:
                return True
            for item in inf:
                if Callback!=None:
                    Callback(item)
            items.extend(inf)
            return False

        return self.ListIterator("search/search"+str(type),{"query":keyword},BodyBlock=foo,SinceRetrieveKeyName=None,SinceSetKeyName=None,**kwargs)
    def detailWorker(self,URL,InfoParams):
        '''
        This is a wrapper around POST() to solve various overheads like:
            Locked Work
            Follower-Only Content
        '''
        data=None
        try:
            data=json.loads(self.POST(URL,InfoParams,timeout=self.Timeout).content)
        except:
            return None
        UnicodeStatus=None
        try:
            UnicodeStatus=unicode(data["data"]["data"]).encode('utf8')
        except:
            try:
                UnicodeStatus=data["data"]["data"]
            except:
                pass
        if UnicodeStatus==BCYCore.NeedFollowServerResponse:
            UID=int(data["data"]["profile"]["uid"])
            self.followUser(UID)
            try:
                data=json.loads(self.POST(URL,InfoParams,timeout=self.Timeout).content)
            except:
                self.unfollowUser(UID)
                return None
            self.unfollowUser(UID)
        if UnicodeStatus==BCYCore.LockedServerResponse:
            return None
        return data["data"]

    def illustDetail(self,dp_id,rp_id):
        return self.detailWorker("illust/detail",{"dp_id":str(dp_id),"rp_id":str(rp_id)})
    def cosDetail(self,cp_id,rp_id):
        return self.detailWorker("coser/detail",{"rp_id":str(rp_id),"cp_id":str(cp_id)})
    def dailyDetail(self,ud_id):
        return self.detailWorker("daily/detail",{"ud_id":str(ud_id)})
    def groupPostDetail(self,GroupID,PostID):
        return self.detailWorker("group/postDetail",{"gid":GroupID,"post_id":PostID})
    def circleList(self,CircleID,Filter,**kwargs):
        return self.ListIterator("circle/work",{"id":CircleID,"filter":Filter},**kwargs)
    def tagList(self,TagName,Filter,**kwargs):
        return self.ListIterator("circle/tag",{"name":TagName,"filter":Filter},**kwargs)
    def groupPostList(self,GroupID,Filter,**kwargs):
        return self.ListIterator("group/listPosts",{"gid":GroupID,"filter":Filter},**kwargs)
    def userWorkList(self,UID,Filter,**kwargs):
        Param={"uid":UID,"num":10,"filter":"origin","source":Filter,"type":"user"}
        items=list()
        try:
            items.extend(json.loads(self.POST("timeline/latest",Param,timeout=self.Timeout).content)["data"])
        except:
            return items
        try:
            for foo in items:
                kwargs["Callback"](foo)
        except:
            pass
        ParamCopy=deepcopy(Param)
        ParamCopy["since"]=int(time.time());
        moreItems=self.ListIterator("timeline/more",ParamCopy,SinceRetrieveKeyName="tl_id",SinceSetKeyName="since",**kwargs);
        items.extend(moreItems)
        return items
    def likedList(self,Filter,**kwargs):
        return self.ListIterator("space/zanlist",{"sub":Filter},**kwargs)
    def WorkTypeCircle(self,name):
        '''
        name is the column name used on the website banner, possible values:
            绘画
            COS
            写作
            漫展
            讨论
        '''
        data=self.POST("tag/relaCircle",{"name":name},timeout=self.Timeout).content
        try:
            data=json.loads(data)
            return data
        except:
            return None
    def reportWork(self,Info,WorkType,Reason):
        '''
        Info is the WorkInfo, containing WorkIdentifiers *ONLY*
        Reason is the descriptive string used globally. For now there are:
            不宜公开讨论的政治问题
            广告等垃圾信息
            不友善内容
            色情／血腥／暴力等违规内容
            盗用／抄袭他人作品
            其他
        '''
        Args=dict()
        Args.update(Info)
        Args["type"]=Reason
        return self.POST(WorkType+"/denouncePost",Args)
    def userRecommends(self,UID,Filter,Callback=None):
        '''
        Download User Recommends List
        '''
        #Implement our own iterator because ListIterator needs to have some hack to work with this
        retVal=list()
        nextSince=None
        Param={"uid":str(UID),"filter":"tuijian","source":Filter}
        try:
            while True:
                PA=deepcopy(Param)
                if nextSince!=None:
                    PA["since"]=nextSince
                Rval=self.POST("timeline/userGrid",PA).content
                Rval=json.loads(Rval)["data"]
                if len(Rval)==0:
                    return retVal
                for item in Rval:
                    if Callback!=None:
                        Callback(item)
                    if int(item["tl_id"])<nextSince or nextSince==None:
                        nextSince=int(item["tl_id"])
                retVal.extend(Rval)
        except:
            return retVal
        return retVal
    def likeWork(self,WorkType,Info):
        '''
        Info is the WorkInfo, containing WorkIdentifiers *ONLY*
        '''
        return self.POST(WorkType+"/doZan",Info)
    def unlikeWork(self,WorkType,Info):
        '''
        Info is the WorkInfo, containing WorkIdentifiers *ONLY*
        '''
        return self.POST(undoZan+"/undoZan",Info)
    def queryDetail(self,Info):
        '''
        This method:
            1. Analyze Info
            2. Extract Values and dispatch them to corresponding APIs
            3. Return the result
        It's usually better to call this method instead of directly using the underlying APIs
        '''
        try:
            Info=Info["detail"]
        except:
            pass
        if "ud_id" in Info.keys():
            return self.dailyDetail(Info["ud_id"])
        elif "cp_id" in Info.keys():
            return self.cosDetail(Info["cp_id"],Info["rp_id"])
        elif "dp_id" in Info.keys():
            return self.illustDetail(Info["dp_id"],Info["rp_id"])
        elif "post_id" in Info.keys() and "gid" in Info.keys():
            return self.groupPostDetail(Info["gid"],Info["post_id"])
        else:
            with tempfile.NamedTemporaryFile(delete=False, suffix="PyBCYDetail-") as outfile:
                json.dump(Info, outfile)
                raise Exception('QueryDetail is called with unsupported Info. Written To:', outfile.name)
