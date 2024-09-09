#!/bin/bash

current_dir=$(dirname "$(realpath "$0")")
node_list=${current_dir}/nodelistbmc      #BMC IP
username=Administrator                   #BMC用户
password=Admin@9000                      #BMC用户密码
sum=20									 #抓取次数

# 定义编译后的可执行文件的名称
EXECUTABLE="powerget"

# ip_address保存BMC的ip地址
readarray -t ip_address < ${node_list}
count=${#ip_address[@]}

echo -e "\n>>> Start to grab power consumption>>>"
echo -e "--------------------------------------------------------------------------"
while [ $sum -gt 0 ]; do
   for (( i=0; i<$count; i++ )); do
      #取出当前ip
      ip=${ip_address[$i]}
      echo -e "\n>>> This is the power grabbing for IP: $ip"
      # echo "${current_dir}"
      # 调用 powerget 并传递 IP 地址和时间 time(ms)
      "${current_dir}/rapl/${EXECUTABLE}" --ip "$ip" --time 10000
      # 检查 powerget 程序执行是否成功
      if [ $? -eq 0 ]; then
          echo "Execution successful for IP: $ip"
          echo "=====================================" >> "${current_dir}/rapl/consumption.log"
      else
          echo "Execution failed for IP: $ip"
      fi
   done
   sum=`expr $sum - 1`
   sleep 10            #每隔10秒抓取一次
done

echo -e "--------------------------------------------------------------------------"
echo -e "\n>>>Grabbing finished>>>"

