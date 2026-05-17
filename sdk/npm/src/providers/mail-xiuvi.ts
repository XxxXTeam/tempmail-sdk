import { createSocketIoMailProvider } from './socketio-mail';

const provider = createSocketIoMailProvider('mail-xiuvi', 'mail.xiuvi.cn');
export const generateEmail = provider.generateEmail;
export const getEmails = provider.getEmails;
