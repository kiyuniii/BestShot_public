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

async function parse_metadata(metadata) {
	const parser = new DOMParser();
	const xmlDoc = parser.parseFromString(metadata, "text/xml");
  
	const objects = xmlDoc.getElementsByTagName("tt:Object");
  
	for (let i = 0; i < objects.length; i++) {
	  const imageRefNode = objects[i].getElementsByTagName("tt:ImageRef")[0];
	  if (imageRefNode) {
		const imageRef = objects[i].getElementsByTagName("tt:ImageRef")[0];
		const imagePath = imageRef ? imageRef.textContent : "";
		const tableBody = document.getElementById("table-body");
		const row = document.createElement("tr");
  
		const classCandidate = objects[i].getElementsByTagName("tt:ClassCandidate")[0];
		const type = classCandidate ? classCandidate.getElementsByTagName("tt:Type")[0].textContent : "N/A";
  
		if (type == "Face") {
		  fill_facedata(objects[i], row);
		} else if (type == "Human") {
		  fill_humandata(objects[i], row);
		} else if (type == "Vehical") {
		  fill_VehicleData(objects[i], row);
		}
  
		fetch(imagePath)
		  .then(response => {
			if (!response.ok) {
			  throw new Error('Network response was not ok ' + response.statusText);
			}
			return response.blob();
		  })
		  .then(blob => {
			const imgElement = document.createElement('img');
			imgElement.src = URL.createObjectURL(blob);
			imgElement.alt = "Image";
			imgElement.style.width = "180px";
			imgElement.style.height = "100px";
  
			const attributeCell = document.createElement("td");
			attributeCell.appendChild(imgElement);
			row.appendChild(attributeCell);
			tableBody.appendChild(row);
  
			const ocrTableBody = document.getElementById("ocr-table-body");
			const ocrRow = document.createElement("tr");
  
			if (type === "LicensePlate") {
			  // ANPR 섹션에 이미지와 차량번호 추가
			  const ocrImgCell = document.createElement("td");
			  const ocrCanvas = document.createElement('canvas');
			  ocrCanvas.width = 180; // 초기 크기
			  ocrCanvas.height = 100; // 초기 크기
  
			  const ocrImgElement = new Image();
			  ocrImgElement.src = URL.createObjectURL(blob);
			  ocrImgElement.onload = async function () {
				preprocessImageWithOpenCV(ocrImgElement, ocrCanvas);
  
				// OCR 결과 처리
				const tensor = preprocessImageForOCR(ocrCanvas);
				const ocrResult = await getOCRResult(tensor);
				licenseCell.textContent = ocrResult; // OCR 결과를 표시
  
				/* KIYUN(1121): AUTO-DOWNLOAD 'ocr_result.txt' */
				const blob = new Blob([ocrResult], {type: 'text/plain'});
				const link = document.createElement('a');
				link.href = URL.createObjectURL(blob);
				link.download = 'ocr_result.txt'; 
				document.body.appendChild(link);
				link.click();
				document.body.removeChild(link);
				URL.revokeObjectURL(link.href);
			  };
  
			  ocrImgCell.appendChild(ocrCanvas);
			  ocrRow.appendChild(ocrImgCell);
  
			  const licenseCell = document.createElement("td");
			  ocrRow.appendChild(licenseCell);
  
			  ocrTableBody.appendChild(ocrRow);
			}
		  })
		  .catch(error => {
			console.error('There was a problem with the fetch operation:', error);
		  });
	  }
	}
  }