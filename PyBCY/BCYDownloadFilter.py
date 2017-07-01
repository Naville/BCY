import re
class BCYDownloadFilter(object):
    def __init__(self):
        '''
        Download Filter used by BCYDownloadUtils
        Every list supports compiled regex unless otherwise specified
        - UIDList is a list of int
        - WorkList
        - TagList
        - UserNameList
        - RegexList is a list of compiled regexs running against json dumped info
        '''
        self.UIDList=list()
        self.WorkList=list()
        self.TagList=list()
        self.UserNameList=list()
        self.RegexList=list()
    def ShouldBlockInfo(self,Info):
        for UID in self.UIDList:
            if int(Info['uid'])==UID:
                return True
        for Tag in self.TagList:
            for item in Info["post_tags"]:
                if Tag==item:
                    return True
                elif type(Tag)==type(re.compile("")) and Tag.match(item):
                    return True
        for UserName in self.UserNameList:
            if UserName==Info["profile"]["uname"]:
                return True
            elif type(UserName)==type(re.compile("")) and UserName.match(Info["profile"]["uname"])!=None:
                return True
        for R in self.RegexList:
            if(R.match(json.dumps(Info,separators=(',', ':'),ensure_ascii=False, encoding='utf8'))!=None):
                return True
        return False
