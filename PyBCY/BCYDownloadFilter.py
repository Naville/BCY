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
                print (UID+" AFilted")
                return True
        for Tag in self.TagList:
            for item in Info["post_tags"]:
                if Tag==item:
                    print (item+"BFilted")
                    return True
                elif type(Tag)==type(re.compile("")) and Tag.match(item):
                    print (Tag+"c Filted")
                    return True
        for UserName in self.UserNameList:
            if UserName==Info["profile"]["uname"]:
                print (UserName+"DFilted")
                return True
            elif type(UserName)==type(re.compile("")) and UserName.match(Info["profile"]["uname"])!=None:
                print (UserName+"EFilted")
                return True
        for R in self.RegexList:
            if(R.match(json.dumps(Info,separators=(',', ':'),ensure_ascii=False, encoding='utf8'))!=None):
                print (R+"FFilted")
                return True
        return False
