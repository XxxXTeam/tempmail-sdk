export type Channel = 'tempmail' | 'linshi-email' | 'tempmail-lol' | 'chatgpt-org-uk';

export interface EmailInfo {
  channel: Channel;
  email: string;
  token?: string;
  expiresAt?: string | number;
  createdAt?: string;
}

export interface Email {
  id?: string | number;
  eid?: string;
  _id?: string;
  from?: string;
  from_address?: string;
  from_name?: string;
  address_from?: string;
  name_from?: string;
  to?: string;
  name_to?: string;
  email_address?: string;
  subject?: string;
  e_subject?: string;
  body?: string;
  text?: string;
  content?: string;
  html?: string;
  html_content?: string;
  date?: string | number;
  e_date?: number;
  timestamp?: number;
  received_at?: string;
  created_at?: string;
  createdAt?: string;
  is_read?: number;
  has_html?: boolean;
  [key: string]: any;
}

export interface GetEmailsResult {
  channel: Channel;
  email: string;
  emails: Email[];
  success: boolean;
}

export interface GenerateEmailOptions {
  channel?: Channel;
  duration?: number;
  domain?: string | null;
}

export interface GetEmailsOptions {
  channel: Channel;
  email: string;
  token?: string;
}
