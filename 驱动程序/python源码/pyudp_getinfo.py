'''
作者: 李展旗
Date: 2024-01-10 21:30:55
文件最后编辑者: lizhanqi
LastEditTime: 2024-01-11 00:37:45
'''
import socket,re,psutil,sys
 
before=0
ip=''
# 获取所有CPU的占用信息
def get_cpu_usage():
    cpu_usage = psutil.cpu_percent(interval=1)
    return cpu_usage

def get_delta():#获取下载变化值
    global before
    now = psutil.net_io_counters().bytes_recv
    delta = (now-before)/102400#变成K再除100，大致相当于多少M宽带。
    before = now
    re='{:.2f}Mb'.format(delta)
    return  re #返回改变量

def menu(ip):
    print("\n\n当前推送给对端的ip地址是:\t{:}\n".format(ip))
    print("请选择".center(30))
    print("1,推送本机基本信息")
    print("2,更改主题")
    print("3,更改天气城市")
    print("4,更改天气城市及wifi信息")
    print("5,更改推送对端ip")
    print('q,退出驱动')
    select=input("请输入上述需要进行操作:")
    if select in ['q','bye','exit']:
        print("程序退出！")
        sys.exit(0)
    try:
        select=int(select)
    except:
        return menu()
    return select

def confirm():
    info=input("是否变更当前设置(y/n):")
    if info=="y" or info=='Y':
        return True
    return False


def put_cpu_info(ip,port):
    get_delta()
    print("使用Ctrl+C中断该程序!\n推送中...")
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # socket.SOCK_DGRAM - udp
    try:
        while True:
            d={"cpu":str(get_cpu_usage())+"%",'mem':str(psutil.virtual_memory().percent)+"%",
            'net':get_delta()}
            udp_socket.sendto(str(d).encode('utf-8'), (ip, port))
    except:
        udp_socket.close()
        print("关闭")

def get_config(config=False):
    if config:
        global ip
        ip=input("输入muziclock的ip地址:")
        with open('config.txt','w') as f:
            txt=f.write(ip)
        return ip
    try:
        with open('config.txt','r') as f:
            txt=f.read()
    except:
        print("没有找到配置文件!")
        txt=''
        
    if txt:
        ip=txt
    else:
        ip=input("输入muziclock的ip地址:")
        with open('config.txt','w') as f:
            txt=f.write(ip)
    return ip




class Send_Udp:
    def __init__(self,host,port):
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp_socket.settimeout(3)
        self.host=host
        self.port=port
        self.message={'command':''}
    def get_setting(self):
        self.message['command'] = "setting"

    def get_theme(self):
        self.message['command'] = "theme"

    def change_theme(self):
        self.message['command'] = "changetheme"
    
    def chanage_setting_city(self,city):
        self.message['command'] = 'changesetting'
        self.message['city']=city

    def chanage_settings(self,ssid,passwd,city):
        self.message['command'] = 'changesetting'
        self.message['city']=city
        self.message['ssid']=ssid
        self.message['pass']=passwd

    def send(self):
        try:
            # 向服务器发送消息
            self.udp_socket.sendto(str(self.message).encode(),(self.host, self.port))
            
            # 等待从服务器返回的消息
            data, addr = self.udp_socket.recvfrom(1024)

            # if data.decode()=='ok':
            #     print("设置成功")
            return data.decode()
        except:
            print("会话超时！中止会话！\n请切换到manager管理页面之后再进行操作")
            self.close()

    def close(self):
        self.udp_socket.close()


def main(ip,port):
    select=menu(ip)
    send_udp=Send_Udp(ip,port)
    if select==2:
        send_udp.get_theme()
        req=send_udp.send()
        if req:
            if req=='white':
                print("当前主题为:\t白色")
            else:
                print("当前主题为:\t黑色")
            if confirm():
                send_udp.change_theme()
                send_udp.send()
                print("变更成功!")
    
    elif select==3:
        send_udp.get_setting()
        req=send_udp.send()
        if req:
            city=re.findall("city':'(.*?)'",req)[0]
            print("当前城市为:\t",city)
            if confirm():
                city=input("请填写变更后的城市名:")
                if len(city)!=0:
                    send_udp.chanage_setting_city(city)
                    send_udp.send()
                    print("变更成功!")
    elif select==4:
        send_udp.get_setting()
        req=send_udp.send()
        if req:
            city=re.findall("city':'(.*?)'",req)[0]
            ssid=re.findall("ssid':'(.*?)'",req)[0]
            passwd=re.findall("pass':'(.*?)'",req)[0]
            print("wifi名字:\t",ssid,"\nwifi密码:\t",passwd,'\n城市:\t',city)
            if confirm():
                ssid=input("请填写变更后的wifi名字:")
                passwd=input("请填写变更后的wifi密码:")
                city=input("请填写变更后的城市名:")
                if (len(city)!=0) and (len(ssid)!=0) and (len(passwd)!=0):
                    send_udp.chanage_settings(ssid,passwd,city)
                    send_udp.send()
                    print("变更成功!")
    elif select==5:
        get_config(True)
    elif select==1:
        send_udp.close()
        put_cpu_info(ip,port)
    else:
        send_udp.close()


if __name__ == '__main__':
    port=1122
    ip=get_config()
    while True:
        main(ip,port)


