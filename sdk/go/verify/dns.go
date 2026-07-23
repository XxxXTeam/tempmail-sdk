package main

import (
	"net"
	"strings"
)

/*
 * lookupMX 查询域名的首选 MX 主机，用于辅助判断收信失败是否因 MX 指向第三方
 * 从网站地址中提取注册域名后查询；查询失败返回空字符串。
 */
func lookupMX(website string) string {
	host := website
	host = strings.TrimPrefix(host, "https://")
	host = strings.TrimPrefix(host, "http://")
	if i := strings.IndexByte(host, '/'); i >= 0 {
		host = host[:i]
	}
	host = strings.TrimSpace(host)
	if host == "" {
		return ""
	}
	mxs, err := net.LookupMX(host)
	if err != nil || len(mxs) == 0 {
		return ""
	}
	return strings.TrimSuffix(mxs[0].Host, ".")
}
