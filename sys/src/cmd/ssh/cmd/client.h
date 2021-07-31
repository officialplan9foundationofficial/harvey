/* Used by the client */
extern RSApub		*server_session_key;
extern RSApub		*server_host_key;
extern uchar		 server_cookie[8];

extern RSApriv		*client_session_key;
extern RSApriv		*client_host_key;

extern ulong		 protocol_flags;
extern ulong		 supported_cipher_mask;
extern ulong		 supported_authentications_mask;

extern char		*user;
extern char		*localuser;
extern char		server_host_name[];

extern int		 no_secret_key;
extern int		 verbose;
extern int		 crstrip;
extern int		interactive;
extern int		alwaysrequestpty;
extern int		cooked;
extern int		usemenu;

void			get_ssh_smsg_public_key(void);
void			put_ssh_cmsg_session_key(void);
void			get_ssh_smsg_success(void);
void			put_ssh_cmsg_user(void);
void			user_auth(void);
void			run_shell(char *argv[]);
void			request_pty(void);
