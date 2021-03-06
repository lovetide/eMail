欢迎阅读自我说明

本说明将尽可能说明 email 程序。
首先呢，这个文件将会以问答表的形式出现，因为我已经知道什么样的问题会被问到。
所以，阅读吧。

问： 怎样从 github 检出源代码来获取我所需要的一切？

答： 在命令行中输入以下命令：
    > git clone --recursive git@github.com:deanproxy/eMail.git
    或者，若要获取 lovetide 修改版
    > git clone --recursive https://github.com/lovetide/eMail

问： 'email' 是什么？

答： 'email' 是我设计的一个通过命令行发送邮件到远程 SMTP 服务器（或者内部使用
    'sendmail'）的程序，并且完全与 GnuPG 交互来加密和签名你的邮件，所以你确定要
    这么做的话…… 你可以从下面的网址获得 GnuPG ： http://www.gnupg.org

问： 怎样编译并安装这个东东？

答： 参考以下步骤：
    
    ./configure
    make
    make install # 需要 root 超级用户权限

问： 它会被安装到哪里？

答：执行程序文件名是 'email'，它会被安装到在 ./configure 期间指定的 prefix 或者 bindir 的某个文件夹下。  
    如果你在 configure 时选择指定 prefix，那它就会去 $bindir 文件夹下，此时 $bindir
    就是 $prefix 下的 bin 文件夹。  如果你指定了 --bindir 参数，那它就会被放到你
    所制定的 $bindir 文件夹下。

    如果在 configure 是你没指定 prefix，那它会去往 /usr/local/bin/email。 配置文件
    则默认安装到 /usr/local/etc/email/ 文件夹下。 不过，如果你在 configure 时指定
    了 --sysconfdir 选项，则配置文件会去往 $sysconfdir 文件夹。
    
    请参阅 ./configure --help

问： 怎样让你的这个怪异的程序正常工作？

答：好吧，你应做的第一件事就是配置 email 客户端。
    在 /usr/local/etc/email/ 文件夹下会有一个 email.conf 配置文件。
    默认情况下，一些不太重要的选项没有启用 (address_book, save_sent_mail, 
    temp_dir reply_to, signature_file, signature_divide)，但你可以很容易的手工
    启用这些选项，他们并不是正常运行 email 所必需的选项。

    你会看到它有一些你必须设置的选项。

    1: SMTP_SERVER:	       指定 SMTP 服务器的名称或者 IP 地址
    2: SMTP_PORT:    	   指定 SMTP 服务器的端口
    3: MY_NAME       	   指定你的姓名
    4: MY_EMAIL:     	   指定你的 email 地址
    5: REPLY_TO:     	   指定“回复到” email 地址
    6: SIGNATURE_FILE:	   指定你的签名文件
    7: ADDRESS_BOOK:	   指定你的通讯录文件
    8: SAVE_SENT_MAIL:     指定保存已发送邮件（email.sent 文件）的文件夹
    9: TEMP_DIR:           指定临时文件夹
   10: GPG_BIN:            指定 gpg 可执行文件
   11: GPG_PASS:           可选的 gpg 密码
   12: SMTP_AUTH:          SMTP 验证类型，可选参数值： LOGIN 或者 PLAIN
   13: SMTP_AUTH_USER:     你的 SMTP 验证帐号
   14: SMTP_AUTH_PASS:     你的 SMTP 验证密码
   15: USE_TLS             是否使用 TLS/SSL，可选参数值： true 或者 false
   16: VCARD               指定名片文件，该文件将会附加到邮件中

    SMTP_SERVER can be either a remote SMTP servers fully qualified domain name, or
    an IP address。  You may also opt to use 'sendmail' internally instead of sending
    via remote SMTP servers。  To do this you just put the path to the sendmail
    binary and any options you would like to use with sendmail (Use -t) in the place
    of the smtp server name… HINT: If you would like to send emails to people on
    your local box (i.e. djones@localhost ), then you must use the sendmail binary。


    When you are specifying file paths, you can use the tilde wildcard as you 
    could in the shell to specify your home directory。 Example: ~/.email.conf
    would mean /home/user/.email.conf to the email program。

    Once you are done here, you can leave your email in /usr/local/etc/email/email.conf
    or the directory you specified during the configure with --sysconfdir=…
    for a global configuration, or in your local home directory as ~/.email.conf for
    a personal configuration。  Personal configs override global configs。  

    You can get online help by using the --help option with email and specifying
    the command line option you need help with。  Example: email --help encrypt

    If you use the -encrypt or -sign option, you MUST have GnuPG installed on your system。
    email uses gpg to encrypt the email to the FIRST email recipient

    Example: email -s "This is the subject" -encrypt dean@somedomain.org
    
    in that example, I would be sending the email to dean@somedomain.org and gpg would
    encrypt it with the key of dean@somedomain.org

    You can use -high-priority ( or -o ) to send your message in a high priority
    matter。  In MS Outlook you will see a little '!' mark next to the letter so that
    the recipient will see that the message is high priority！

    You can send a message in one of two ways:
    The first way is to already have a message ready to send。  Say if I have a file named
    "this.txt"  and I want to send it to 'dean@somedomain.org'。  I can redirect this file to
    the email program in one of two ways。  Example below:

    cat this.txt | email -s "Sending this.txt to you" dean@somedomain.org
    or
    email -s "Sending this.txt to you" dean@somedomain.org < this.txt

    If you want to create a message, you will need to do two things here。
    First set the environment variable "EDITOR" to your favorite editor。 

    Example: 'export EDITOR=vi'

    Please use your favorite editor in place of vi。
    Now all you have to do is execute the example below:

    Example: email -s "Subject" dean@somedomain.org

    This will open up your favorite editor and let you write a email to dean@somedomain.org
    email will default to 'vi' if you do not set EDITOR。

    You can send to multiple recipients with 'email'。  All you have to do is
    put commas between the email addresses you want the message to be sent to。
    Example below:
    dean@somedomain.org,another@domain。com,you@domain。com 

    Here are some more examples below:

    Example: the below command will send a message that is encrypted with 'dean@somedomain.org' key
    email -s "my email to you" -encrypt dean@somedomain.org,software@cleancode.org

    Example: the example will sign the message directed to it。
    email -s "signed message" -sign dean@somedomain.org < secret_stuff.txt

    Example: This will send to multiple recipients 
    email -s "To all of you" dean@somedomain.org,you@domain。com,me@cleancode.org 

    Example: Set message to high priority
    email -s "High priority email" -high-priority dean@somedomain.org

    Example: Send message with 2 attachements
    email -s "here you go…" -attach file -attach file2 dean@somedomain.org

    Example: Add headers to the message
    email -s "New Message" --header "X-My-Header: Stuff" \
      --header "X-Another-Header: More Stuff" dean@somedomain.org

问： 能使用签名吗？

答： 是的，我们能。
    Look in email.conf and edit the signature variables as needed。
    If you're wondering what a signature divider is, it's the little
    thingy that divides your email message from the signature。
    Usually it's '---' (Default)

    Also, you can specify wild cards in the signature file。  
    %c = Formated time, date, timezone ( looks like the output of 'date' )
    %t = Time only ( US Standard format )
    %d = Date Only ( US Standard format )
    %v = Version ( For us folks who want to endorse 'email' )
    %h = Host type (ex。 Linux 2。2。19 i686 )
    %f = Prints output from the 'fortune(6)' command
    %% = Prints a % mark

    Your sig could look like this:
    ---
    This message was sent: %c
	
    This would end up looking like: 
	This message was sent: Thu Dec 13 04:54:52 PM EST 2001

问： 通讯录是怎样工作的？

答：  Set up your email.conf file to point to your very own address book。
    There is a template in the email source directory that you can view to set up your 
    own address book。  The format should be as below:

	Any single name to email translation will have to have a 'single:' token before it:
	  single: Software    = software ^at^ cleancode.org
	  single: Dean        = dean@somedomain.org
	  single: "Full Name" = someone@somedomain.org

	Any group name to email translation will have to have a 'group:' token before it: 
	With groups, you can only use the Names of your single statements above… Format below:
	  group: Both = Software,Dean

    See the email。address。template file for more information


问： 能发附件吗？

答：  这个可以有！ 我们现在支持发附件了！
    你只要把你要附加的文件用 --attach 参数指定即可，多个文件可用逗号分割。
    所有文件将会用 base64 编码并附加上合适的 MIME 头信息。
    
    举例：
    
      email -s 单个附件测试 --attach 某个文件 dean@somedomain.org

      # 多个文件
      email -s 多个附件测试 --attach 文件1 --attach 文件2 dean@somedomain.org
      

问： 能进行 SMTP 验证吗？

答： 是的！ Email 能进行 SMTP 验证。 你需要在 email.conf 配置文件中设置少许选项。
    SMTP_AUTH, SMTP_AUTH_USER 以及 SMTP_AUTH_PASS。 如果你了解更多，请查看 email
    手册 'man email'。


问： 我能加入开发团队吗？

答： 你能, 从 http://www.cleancode.org/projects/email 发邮件并问怎样加入，或者就用 git 
    克隆 email 源代码 (参见上面的说明) 然后就可以开始编程并提交了！

问： 为什么要用 email？

答： 因为 'mailx' 不会将邮件发送外部 SMTP 服务器并且我不能访问 sendmail。
    我需要一个能与外部 SMTP 服务器通信，并且还能一步加密好我的消息的东西，不用我
    去手工采取许多步骤才能加密好。

问： 'email' 代表什么意思？

答： 好吧，尽管含义很流行，但它代表 "安全的邮件" 而不是 "电子邮件"
    我的最初目的是想让从命令行发加密邮件更容易些，不用去处理那些 'mailx' 要你去处理
    的所有 TMD 琐碎的步骤！   抱歉啦 mailx！  (mailx: 囧)

问： 开发人员是谁？

答：  Dean Jones - 主开发者

就这些了。
我希望你喜欢 'email' 程序。

如果你有任何问题、程序缺陷或者其他想法，请在下面的网址给我们发邮件：
http://www.cleancode.org/projects/email/contact

乐邮邮，邮邮乐！
