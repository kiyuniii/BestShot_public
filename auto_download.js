function getData(Response)
{
  const filePathRegex = /download\/veda_team_36_num2_final\/[^\s]+/;
  const match = Response.match(filePathRegex);

  if (match && match[0]) {
    let filePath = match[0];
    filePath = '/' + filePath;
    // console.log("file path:", filePath);


  /* KIYUN(1121): AUTO-DOWNLOAD 'raw_bestshot.jpg' */
  /* KIYUN(1121): AUTO-DOWNLOAD 'metadata.txt' */
    const isImage = filePath.toLowerCase().endsWith('.jpg') || filePath.toLowerCase().endsWith('.jpeg');

	if(isImage) {
		fetch(filePath)
		.then(response => {
			if(!response.ok) {
				throw new Error(`HTTP error! status: ${response.status}`);
			}
			return response.blob();
		})
		.then(buffer => {
			const blob = new Blob([buffer], { type: 'image/jpeg' });
			const link = document.createElement('a');
			link.href = URL.createObjectURL(blob);
			link.download = 'raw_bestshot.jpg';
			document.body.appendChild(link);
			link.click();
			document.body.removeChild(link);
			URL.revokeObjectURL(link.href);
			})
			.catch(error => {
				console.error("error when read file:", error);
			});
	} else {
	    fetch(filePath)
    	.then(response => {
         	 if (!response.ok) {
        	      throw new Error(`HTTP error! status: ${response.status}`);
       		 }
			return response.text();
		})
		.then(data => {
        	// console.log("file data:", data);
			parse_metadata(data);

			const blob = new Blob([data], { type: 'text/plain' });
			const link = document.createElement('a');
			link.href = URL.createObjectURL(blob);
			link.download = 'metadata.txt';
			link.click();
			URL.revokeObjectURL(link.href);
		})
		.catch(error => {
	    	console.error("error when read file:", error);
		});
	}}
}
