FROM hub.byted.org/compile/milvus-base:ubuntu20.04-conan
ENV TZ=Asia/Shanghai
RUN apt-get update && apt-get install -y systemd
RUN wget -qO- "https://cmake.org/files/v3.26/cmake-3.26.5-linux-$(uname -m).tar.gz" | tar --strip-components=1 -xz -C /usr/local
RUN mkdir -p /root/conan_server
RUN cp -rf /root/.conan/data /root/conan_server
# 创建用户 tiger
RUN useradd -m tiger
# 设置用户 tiger 的密码（可选，为了安全起见，建议使用更安全的密码设置方法）
# 可以通过 docker-compose.yml 中的 secrets 或者 Docker 的 secrets 功能来管理密码
# RUN echo 'tiger:tigerpassword' | chpasswd
# 创建目录 /opt/tiger
RUN mkdir -p /opt/tiger
RUN chown -R tiger:tiger /opt/tiger
# 创建目录 /var/log/tiger
RUN mkdir -p /var/log/tiger
RUN chown -R tiger:tiger /var/log/tiger
# 创建目录 /opt/log/tiger
RUN mkdir -p /opt/log/tiger
RUN chown -R tiger:tiger /opt/log/tiger
RUN echo "build version:ubuntu20.04-conan" > version.txt