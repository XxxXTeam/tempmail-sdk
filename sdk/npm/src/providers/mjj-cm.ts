import { createSocketIoMailProvider } from "./socketio-mail";

const provider = createSocketIoMailProvider("mjj-cm", "mjj.cm");
export const generateEmail = provider.generateEmail;
export const getEmails = provider.getEmails;
