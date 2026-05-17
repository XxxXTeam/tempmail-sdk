import { createSocketIoMailProvider } from './socketio-mail';

const provider = createSocketIoMailProvider('linshi-co', 'linshi.co');
export const generateEmail = provider.generateEmail;
export const getEmails = provider.getEmails;
