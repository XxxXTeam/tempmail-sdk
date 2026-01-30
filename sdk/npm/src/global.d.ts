declare global {
  function fetch(input: RequestInfo | URL, init?: RequestInit): Promise<Response>;
}

export {};
