# 工具栏与贴边缩放修订

## 用户要求

彻底删除更多菜单和重复的最大化／隐藏按钮，收藏置于地址栏中，工具栏按钮始终可达，地址栏占剩余宽度。滚轮调窗根据当前贴边或新碰到的边界调整锚点。继续不运行应用、自动测试或截图测试，仅编译和打包。

## 实现

- 系统标题栏承担最大化／还原／最小化／关闭，包括最大化时保留系统标题栏。
- 工具栏固定保留六个操作，400 DIP以下地址栏与按钮分行，200 DIP以下按钮再分一行；不再折叠到更多菜单。
- 收藏星标位于统一地址栏边框内，网址使用Enter导航。
- 调窗仍使用5%比例；按设置中的吸附距离识别贴边。贴角固定角，单边固定该轴、另一轴居中；越界即贴到新边，最大尺寸受当前显示器工作区约束。
- 无法同时满足最小尺寸、比例与工作区时，沿用工作区可见与最小尺寸优先的策略。

## 杀毒提示

用户确认360提示由小窗浏览器发起，目标为Windows Terminal。源码未找到显式远程线程注入API。全局低级键盘钩子不是进程内注入；输入保护中的UI Automation跨进程查询是待排查路径，尚不能认定为唯一触发源。

Windows Terminal和传统控制台现在通过窗口类型直接识别为输入状态，绕过UI Automation；后台过期焦点请求也在查询前丢弃。代价：终端获得焦点期间按输入状态保护，即使当时仅浏览输出。未修改安全软件或信任设置，未提交样本至第三方。是否消除360提示由用户确认。

参考：
- https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc
- https://github.com/microsoft/terminal/blob/main/src/cascadia/WindowsTerminal/IslandWindow.cpp
- https://learn.microsoft.com/en-us/defender-xdr/developer-faq
