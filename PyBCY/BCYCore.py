# -*- coding: utf-8 -*-
import base64
import json
import os
import requests
import sys
import tempfile
import threading
import binascii
import string
import random
import urllib3
from Crypto.Cipher import AES
from sys import version as python_version
import time
try:
    reload(sys)
    sys.setdefaultencoding('utf8')
except:
    pass
if python_version.startswith('3'):
    from urllib.parse import urlparse
else:
    from urlparse import urlparse

def id_generator(size=6, chars=string.ascii_uppercase + string.digits):
    return ''.join(random.choice(chars) for _ in range(size))
class BCYCore(object):
    '''
    对所有列表类API而言,我们在Progress里存入本次迭代所找到的最新作品时间戳或其他标志位。下次迭代时遇到比该作品更旧的作品时说明到头了可跳出。
    可能的Filter值:
         - coser
         - all
         - drawer
         - writer
         - daily
         - zhipin
    可能的WorkType值:
         - illust
         - coser
         - daily
         - group
         - circle
         - group

    所有的作品都由一组全局唯一的标识符标记。不同类型的作品所用的标识符Key不同
    自2017年7月左右起半次元API接口开启了同IP的并发请求限制。请求过快或过密会导致服务器不返回正文或者空正文。IP地址也会被Ban
    前者会触发requests.exceptions.ConnectionError异常
    '''
    APIBase = "https://api.bcy.net/"
    Header = {"User-Agent": "bcy 4.3.2 rv:4.3.2.6146 (iPad; iPhone OS 9.3.3; en_US) Cronet"}
    QueryBase = {"version_code":"4.3.2",
                "mix_mode":"1",
                "account_sdk_source":"app",
                "language":"en-US",
                "vid":id_generator(size=8)+"-"+id_generator(size=4)+"-"+id_generator(size=4)+"-"+id_generator(size=4)+id_generator(size=12),
                "device_id":id_generator(size=11, chars=string.digits),
                "channel":"App Store",
                "resolution":"2880*1800",
                'aid':"1250",
                "screen_width":"2880",
                "openudid":id_generator(size=40),
                "device_id":id_generator(size=12, chars=string.digits),
                "os_api":"18",
                "ac":"WIFI",
                "os_version":"16.0.0",
                "device_platform":"iphone",
                "device_type":"iPhone14,5",
                "idfa":id_generator(size=8)+"-"+id_generator(size=4)+"-"+id_generator(size=4)+"-"+id_generator(size=4)+id_generator(size=12)
                }
    APIErrorResponse = "半次元不小心滑了一跤"
    NeedLoginResponse = "阿拉，请先登录半次元"
    StatusMap = {100: "请先登录半次元",
                 4000: "请先登录半次元",
                 21: "半次元不小心滑了一跤",
                 4010:"阿拉，请先登录半次元",
                 4050:"该讨论已被管理员锁定",
                 160:"半次元在忙，请稍后再试"
                 }
    def __init__(self,Timeout=15):
        self.SessionKey = None
        self.UID=None
        self.Crypto = AES.new(b"com_banciyuan_AI", AES.MODE_ECB)
        self.session = requests.Session()
        self.session.headers.update(BCYCore.Header)
        self.session.verify = False
        urllib3.disable_warnings()
        self.Timeout=Timeout

    def loginWithEmailAndPassWord(self, email, password):
        '''
        登录并非必须。
        但匿名模式下某些API无法使用
        '''
        data = self.POST("passport/email/login/", {"password": password, "email": email},Encrypt=False).content
        data = json.loads(data)["data"]
        self.SessionKey=data["session_key"]
        data=self.POST("api/token/doLogin",{},Encrypt=True).content
        data = json.loads(data)["data"]
        self.UID=data["uid"]
        print("Logged in as:"+data["uname"])
    def EncryptData(self, Data):
        #PKCS7 from https://stackoverflow.com/a/14205319 with modifications
        if sys.version_info >= (3,0):
            Data = Data.encode("utf8")
            length = 16 - (len(Data) % 16)
            Data += bytes([length])*length
        else:
            Data = Data.decode("utf8").encode("utf8")
            length = 16 - (len(Data) % 16)
            Data += chr(length)*length
        return self.Crypto.encrypt(Data)

    def EncryptParam(self, Params):
        ParamData = None
        try:
            ParamData = json.dumps(Params, separators=(',', ':'), ensure_ascii=False, encoding='utf8')
        except TypeError:
            ParamData = json.dumps(Params, separators=(',', ':'), ensure_ascii=False)
        return {"data": base64.b64encode(self.EncryptData(ParamData))}
    def mixHEXParam(self,Params):
        ret=dict()
        for Key in Params.keys():
            Val=Params[Key]
            Crypted=''.join(chr(ord(num) ^ 5) for num in Val)
            Hexed=''.join("{:02x}".format(ord(c)) for c in Crypted)
            ret[Key]=Hexed
        return ret

    def GET(self, URL, Params,Headers=dict(),**kwargs):
        return self.session.get(URL, params=Params,headers=Headers,timeout=self.Timeout,**kwargs)

    def POST(self, URL, Params, Auth=True,Encrypt=True, **kwargs):
        if URL.startswith("http") == False:
            URL = BCYCore.APIBase + URL
        if Auth == True:
            if 'session_key' not in Params.keys() and self.SessionKey != None:
                Params['session_key'] = self.SessionKey
        if Encrypt:
            Params = self.EncryptParam(Params)
        else:
            Params = self.mixHEXParam(Params)
        return self.session.post(URL, params=BCYCore.QueryBase,data=Params,timeout=self.Timeout, **kwargs)

    def userDetail(self, UID):
        return json.loads(self.POST("api/user/detail", {"uid": UID}).content)["data"]

    def downloadWorker(self, URL,Params=dict(),Headers=dict(),Callback=None):
        Content = None
        req = self.GET(URL,Params,Headers=Headers,stream=True)
        total_length = int(req.headers.get('Content-Length'))
        if req == None:
            return None
        if Callback == None:
            Data = req.content
            if (len(Data) == total_length):
                return Data
            else:
                return None
        Downloaded = 0
        for data in req.iter_content(chunk_size=204800):
            Downloaded += len(data)
            if Content == None:
                Content = data
            else:
                Content += data
            Callback(URL, Downloaded, total_length)
        if Downloaded == total_length:
            Callback(URL, Downloaded, total_length)
            return Content
        else:
            return None
    def image_postCover(self,item_id,type):
        '''
        获得带水印的图片URL列表
        '''
        try:
            ImageData = self.POST("/api/image/postCover/",{"id":item_id,"type":type}).content
            ImageData=json.loads(ImageData)["data"]
            return ImageData
        except Exception as exc:
            print(exc)
            return None
    def imageDownload(self, ImageInfo, Callback=None):
        '''
        从指定图片URL下载图片
        默认情况下下载无水印大图。失败的情况从官方API下载有水印大图
        '''
        ImageData = None
        URL = ImageInfo["url"]
        OriginalImageURL = os.path.split(URL)[0]
        try:
            ImageData = self.downloadWorker(OriginalImageURL, Callback=Callback)
        except requests.exceptions.MissingSchema:  # API有Bug.有时图片列表会出现随机文本URL
            return None
        try:
            rep = json.loads(ImageData)
            return None
        except:  # 否则说明是Document Not Found应答
            return ImageData
        ImageData = self.POST("image/download", ImageInfo).content
        rep = json.loads(ImageData)
        if type(rep["data"]) == str:
            return None
        ActualURL = rep["data"]["cover"]
        return self.downloadWorker(ActualURL, Callback=Callback)

    def followUser(self, uid):
        data = json.loads(self.POST("api/user/follow", {"uid": uid, "type": "dofollow"}).content)
        return int(data["status"]) == 1

    def unfollowUser(self, uid):
        data = json.loads(self.POST("api/user/follow", {"uid": uid, "type": "unfollow"}).content)
        return int(data["status"]) == 1

    def detailWorker(self, URL, InfoParams):
        '''
        对POST的简单封装。用于解决:
            - 关注可见
            - 已锁定内容
        '''
        data = json.loads(self.POST(URL, InfoParams).content)
        if data["status"]==4010:
            UID = int(data["data"]["profile"]["uid"])
            self.followUser(UID)
            data = json.loads(self.POST(URL, InfoParams).content)
            self.unfollowUser(UID)
        if data["status"]==4050:
            return None
        return data["data"]
    def likeWork(self,item_id):
        '''
        推荐某个作品。Info为*只*包含标识符的字典
        '''
        Info=dict()
        Info["item_id"]=item_id
        return json.loads(self.POST("/api/item/doPostLike", Info).content)

    def unlikeWork(self,item_id):
        '''
        取消推荐某个作品。Info为*只*包含标识符的字典
        '''
        Info=dict()
        Info["item_id"]=item_id
        return json.loads(self.POST("/api/item/cancelPostLike", Info).content)

    def queryDetail(self, item_id):
        '''
        这个方法为作品的详情查询封装。
        分析Info后分发给相应类型的API并返回结果
        大多数时候你应该调用这个而不是直接调用底层API
        '''

        return self.detailWorker("/api/item/detail/",{"item_id":str(item_id)})
    def tagStatus(self, TagName):
        return json.loads(self.POST("api/tag/status", {"name": TagName}).content)
    def WorkTypeCircle(self, name):
        '''
        name是网站banner上的名称，目前有:
            绘画
            COS
            写作
            漫展
            讨论
        '''
        return json.loads(self.POST("tag/relaCircle", {"name": name}).content)
    def circle_filterlist(self,circle_id,circle_type,circle_name):
        '''
        返回一个圈子可用的filter列表，用于传给search_itemByTag
        '''
        return json.loads(self.POST("/apiv2/circle/filterlist/", {"circle_id": circle_id,"circle_type":circle_type,"circle_name":circle_name}).content)
    def search_itemByTag(self,TagNames,ptype):
        items = list()
        p = 1
        while True:
            Params = {"p": p, "ptype": int(ptype), "tag_list": TagNames}
            p = p + 1
            foo = json.loads(self.POST("/apiv2/search/item/bytag/", Params).content)["data"]
            if Callback != None:
                for i in foo:
                    if Callback(i)==True:
                        return items
            if len(foo) == 0:
                return items
            items.extend(foo)
    def circle_itemhottags(self,item_id):
        return json.loads(self.POST("/apiv2/circle/itemhottags/", {"item_id": item_id}).content)
    def circle_itemrecenttags(self,TagName,Filter,Callback=None):
        items = list()
        since="0"
        try:
            while True:
                A={"since":since,"grid_type":"timeline","filter":Filter,"name":TagName}
                itemList = json.loads(self.POST("/api/circle/itemRecentTags/",A).content)[
                    "data"]
                if len(itemList) == 0:
                    return items
                for item in itemList:
                        if Callback != None:
                            if Callback(item)==True:
                                return items
                        items.append(item)
                since=itemList[-1]["since"]
        except (requests.exceptions.ConnectionError):
            return items
    def getReply(self, item_id, **kwargs):
        '''
        获取某个作品的回复列表
        Params是作品标识符字典
        '''
        items = list()
        p = 1
        while True:
            Par = {"p": p,"item_id":item_id}
            p = p + 1
            foo = json.loads(self.POST("/api/item/getReply", Par).content)["data"]
            if Callback != None:
                for i in foo:
                    if Callback(i)==True:
                        return items
            if len(foo) == 0:
                return items
            items.extend(foo)
    def circle_itemrecentworks(self,WorkID,Filter,Callback=None):
        '''
        参数可以是{"id":作品编号}
        '''
        items = list()
        since="0"
        try:
            while True:
                A={"uid": self.UID, "since":since,"grid_type":"timeline","filter":Filter,"id":WorkID}
                itemList = json.loads(self.POST("/api/circle/itemRecentWorks",A).content)[
                    "data"]
                if len(itemList) == 0:
                    return items
                for item in itemList:
                        if Callback != None:
                            if Callback(item)==True:
                                return items
                        items.append(item)
                since=itemList[-1]["since"]
        except (requests.exceptions.ConnectionError):
            return items
    def userWorkList(self,UID,Callback=None):
        items = list()
        since="0"
        try:
            while True:
                itemList = json.loads(self.POST("/api/timeline/getUserPostTimeLine/", {"uid": UID, "since":since,"grid_type":"timeline"}).content)[
                    "data"]
                if len(itemList) == 0:
                    return items
                since=itemList[-1]["since"]
                items.extend(itemList)
                if Callback != None:
                    for item in itemList:
                        if Callback(item)==True:
                            return items
        except (requests.exceptions.ConnectionError):
            return items
    def likedList(self,UID,Callback=None):
        '''
        下载自己点过赞的作品
        不支持迭代状态保存
        '''
        items = list()
        since="0"
        try:
            while True:
                itemList = json.loads(self.POST("api/space/getUserLikeTimeLine/", {"uid":UID, "since":since,"grid_type":"grid"}).content)[
                    "data"]
                if len(itemList) == 0:
                    return items
                since=itemList[0]["since"]
                items.extend(itemList)
                if Callback != None:
                    for item in itemList:
                        if Callback(item)==True:
                            return items
        except (requests.exceptions.ConnectionError):
            return items



    def searchContent(self, keyword, type, Progress=None, Callback=None, *kwargs):
        '''
        type的可能值:
                Content
                Works
                Tags
                User
        不支持迭代状态保存
        '''
        items = list()
        p = 1
        while True:
            itemList = json.loads(
                self.POST("/api/search/search" + str(type)+"/", {"query": keyword, "p": p}).content)[
                "data"]["results"]
            p = p + 1
            if len(itemList) == 0:
                return items
            items.extend(itemList)
            if Callback != None:
                for item in itemList:
                    if Callback(item)==True:
                        return items
    def groupPostList(self, GroupID, Progress=dict(), Callback=None):
        '''
        这个是用来给定一个讨论串用来获取所有回贴列表的。例如41709对应https://bcy.net/group/list/41709
        '''
        items = list()
        p = 1
        while True:
            Params = {"p": p, "gid": str(GroupID), "type": "ding", "limit": 50}
            p = p + 1
            foo = json.loads(self.POST("/api/group/listPosts", Params).content)["data"]
            if Callback != None:
                for i in foo:
                    if Callback(i)==True:
                        return items
            if len(foo) == 0:
                return items
            items.extend(foo)
    def groupDetail(self,gid):
        return json.loads(self.POST("/api/group/detail/", {"gid": gid}).content)["data"]
    def friendfeed(self,Callback=None):
        '''
        获取当前用户的推送列表
        可在https://bcy.net/home/account/settings的首页设置下进行设置
        '''
        since = str(time.time())
        items = list()
        Init=json.loads(self.POST("apiv2/timeline/friendfeed", {"since":since,"grid_type":"timeline","direction":"refresh"}).content)["data"]
        items.extend(Init)
        if len(items) == 0:
            return items
        since=Init[-1]["since"]
        try:
            Latest = json.loads(self.POST("apiv2/timeline/friendfeed", {"since":since,"grid_type":"timeline","direction":"loadmore"}).content)["data"]
            if len(Latest) == 0:
                return items
            items.extend(Latest)
            since=items[-1]["since"]
            for x in items:
                if Callback != None:
                    if Callback(x)==True:
                        return items
        except:
            return items
